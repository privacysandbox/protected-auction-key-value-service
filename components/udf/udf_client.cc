// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/udf/udf_client.h"

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "components/errors/error_tag.h"
#include "google/protobuf/util/json_util.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/util/duration.h"

namespace kv_server {

namespace {

enum class ErrorTag : int {
  kCodeUpdateTimeoutError = 1,
  kUdfExecutionTimeoutError = 2
};

using google::protobuf::json::MessageToJsonString;
using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::InvocationStrRequest;
using google::scp::roma::kTimeoutDurationTag;
using google::scp::roma::ResponseObject;
using google::scp::roma::sandbox::roma_service::RomaService;

// Roma IDs and version numbers are required for execution.
// We do not currently make use of IDs or the code version number, set them to
// constants.
constexpr char kCodeObjectId[] = "id";
constexpr char kInvocationRequestId[] = "id";
constexpr int kUdfInterfaceVersion = 1;

class UdfClientImpl : public UdfClient {
 public:
  explicit UdfClientImpl(Config<std::weak_ptr<RequestContext>>&& config =
                             Config<std::weak_ptr<RequestContext>>(),
                         absl::Duration udf_timeout = absl::Seconds(5),
                         absl::Duration udf_update_timeout = absl::Seconds(30),
                         int udf_min_log_level = 0)
      : udf_timeout_(udf_timeout),
        udf_update_timeout_(udf_update_timeout),
        udf_min_log_level_(udf_min_log_level),
        roma_service_(std::move(config)) {}

  // Converts the arguments into plain JSON strings to pass to Roma.
  absl::StatusOr<std::string> ExecuteCode(
      const RequestContextFactory& request_context_factory,
      UDFExecutionMetadata&& execution_metadata,
      const google::protobuf::RepeatedPtrField<UDFArgument>& arguments,
      ExecutionMetadata& metadata) const {
    execution_metadata.set_udf_interface_version(kUdfInterfaceVersion);
    std::vector<std::string> string_args;
    string_args.reserve(arguments.size() + 1);
    std::string json_metadata;
    if (const auto json_status =
            MessageToJsonString(execution_metadata, &json_metadata);
        !json_status.ok()) {
      return json_status;
    }
    string_args.push_back(json_metadata);

    for (int i = 0; i < arguments.size(); ++i) {
      const auto& arg = arguments[i];
      const google::protobuf::Message* arg_data;
      if (arg.tags().values().empty()) {
        arg_data = &arg.data();
      } else {
        arg_data = &arg;
      }
      std::string json_arg;
      if (const auto json_status = MessageToJsonString(*arg_data, &json_arg);
          !json_status.ok()) {
        return json_status;
      }
      string_args.push_back(json_arg);
    }
    return ExecuteCode(request_context_factory, std::move(string_args),
                       metadata);
  }

  absl::StatusOr<std::string> ExecuteCode(
      const RequestContextFactory& request_context_factory,
      std::vector<std::string> input, ExecutionMetadata& metadata) const {
    std::shared_ptr<absl::Status> response_status =
        std::make_shared<absl::Status>();
    std::shared_ptr<std::string> result = std::make_shared<std::string>();
    std::shared_ptr<absl::Notification> notification =
        std::make_shared<absl::Notification>();
    auto invocation_request =
        BuildInvocationRequest(request_context_factory, std::move(input));
    PS_VLOG(9, request_context_factory.Get().GetPSLogContext())
        << "Executing UDF with input arg(s): "
        << absl::StrJoin(invocation_request.input, ",");
    ScopeLatencyMetricsRecorder<UdfRequestMetricsContext,
                                kUDFExecutionLatencyInMicros>
        latency_recorder(
            request_context_factory.Get().GetUdfRequestMetricsContext());
    const auto status = roma_service_.Execute(
        std::make_unique<InvocationStrRequest<std::weak_ptr<RequestContext>>>(
            std::move(invocation_request)),
        [notification, response_status,
         result](absl::StatusOr<ResponseObject> response) {
          if (response.ok()) {
            *result = std::move(response->resp);
          } else {
            response_status->Update(std::move(response.status()));
          }
          notification->Notify();
        });
    if (!status.status().ok()) {
      PS_LOG(ERROR, request_context_factory.Get().GetPSLogContext())
          << "Error sending UDF for execution: " << status.status();
      return status.status();
    }

    notification->WaitForNotificationWithTimeout(udf_timeout_);
    if (!notification->HasBeenNotified()) {
      return StatusWithErrorTag(
          absl::Status(absl::StatusCode::kDeadlineExceeded,
                       "Timed out waiting for UDF execution result."),
          __FILE__, ErrorTag::kUdfExecutionTimeoutError);
    }
    if (!response_status->ok()) {
      PS_LOG(ERROR, request_context_factory.Get().GetPSLogContext())
          << "Error executing UDF: " << *response_status;
      return *response_status;
    }
    // TODO(b/338813575): waiting on the K&B team. Once that's
    // implemented we should just use that number.
    metadata.custom_code_total_execution_time_micros =
        absl::ToInt64Milliseconds(latency_recorder.GetLatency());
    return *result;
  }

  absl::StatusOr<absl::flat_hash_map<UniquePartitionIdTuple, std::string>>
  BatchExecuteCode(
      const RequestContextFactory& request_context_factory,
      absl::flat_hash_map<UniquePartitionIdTuple, UDFInput>& udf_input_map,
      ExecutionMetadata& metadata) const {
    absl::flat_hash_map<UniquePartitionIdTuple, std::string> results;
    if (udf_input_map.empty()) {
      PS_VLOG(5, request_context_factory.Get().GetPSLogContext())
          << "UDF input map is empty. Not executing any UDFs.";
      return results;
    }

    UdfRequestMetricsContext& metrics_context =
        request_context_factory.Get().GetUdfRequestMetricsContext();
    LogIfError(metrics_context.LogHistogram<kUDFExecutionCount>(
        (static_cast<double>(udf_input_map.size()))));
    ScopeLatencyMetricsRecorder<UdfRequestMetricsContext,
                                kBatchUDFExecutionLatencyInMicros>
        latency_recorder(
            request_context_factory.Get().GetUdfRequestMetricsContext());

    absl::flat_hash_map<UniquePartitionIdTuple,
                        std::future<absl::StatusOr<std::string>>>
        responses;
    metadata.custom_code_total_execution_time_micros = 0;
    for (auto&& [id, udf_input] : udf_input_map) {
      responses[id] = std::async(
          std::launch::async,
          [this, &request_context_factory, &metadata](UDFInput&& udf_input) {
            ExecutionMetadata single_run_metadata;
            auto result =
                this->ExecuteCode(request_context_factory,
                                  std::move(udf_input.execution_metadata),
                                  udf_input.arguments, single_run_metadata);
            // Record the longest UDF execution time across all parallel
            // executions
            metadata.custom_code_total_execution_time_micros = std::max(
                metadata.custom_code_total_execution_time_micros,
                single_run_metadata.custom_code_total_execution_time_micros);
            return result;
          },
          std::move(udf_input));
    }

    // Process responses
    for (auto&& [id, response] : responses) {
      auto result = response.get();

      if (result.ok()) {
        results[id] = std::move(result.value());
      } else {
        PS_LOG(ERROR, request_context_factory.Get().GetPSLogContext())
            << "UDF Execution failed for partition id " << std::get<0>(id)
            << " and compression_group_id " << std::get<1>(id) << ": "
            << result.status();
      }
    }
    if (results.empty()) {
      return absl::InvalidArgumentError(
          "BatchExecuteCode failed for all UDF invocations.");
    }
    return results;
  }

  absl::Status Init() { return roma_service_.Init(); }

  absl::Status Stop() { return roma_service_.Stop(); }

  absl::Status SetCodeObject(
      CodeConfig code_config,
      privacy_sandbox::server_common::log::PSLogContext& log_context) {
    absl::MutexLock lock(&mutex_);
    // Only update code if logical commit time is larger.
    if (code_object_metadata_.logical_commit_time >=
        code_config.logical_commit_time) {
      PS_VLOG(1, log_context)
          << "Not updating code object. logical_commit_time "
          << code_config.logical_commit_time
          << " too small, should be greater than "
          << code_object_metadata_.logical_commit_time;
      return absl::OkStatus();
    }
    std::shared_ptr<absl::Status> response_status =
        std::make_shared<absl::Status>();
    std::shared_ptr<absl::Notification> notification =
        std::make_shared<absl::Notification>();
    PS_VLOG(9, log_context) << "Setting UDF: " << code_config.js;
    CodeObject code_object =
        BuildCodeObject(std::move(code_config.js), std::move(code_config.wasm),
                        code_config.version);
    absl::Status load_status = roma_service_.LoadCodeObj(
        std::make_unique<CodeObject>(code_object),
        [notification, response_status](absl::StatusOr<ResponseObject> resp) {
          if (!resp.ok()) {
            response_status->Update(std::move(resp.status()));
          }
          notification->Notify();
        });
    if (!load_status.ok()) {
      PS_LOG(ERROR, log_context)
          << "Error setting UDF Code object: " << load_status;
      return load_status;
    }

    notification->WaitForNotificationWithTimeout(udf_update_timeout_);
    if (!notification->HasBeenNotified()) {
      return StatusWithErrorTag(
          absl::Status(absl::StatusCode::kDeadlineExceeded,
                       "Timed out setting UDF code object."),
          __FILE__, ErrorTag::kCodeUpdateTimeoutError);
    }
    if (!response_status->ok()) {
      PS_LOG(ERROR, log_context)
          << "Error compiling UDF code object. " << *response_status;
      return *response_status;
    }
    code_object_metadata_.handler_name =
        std::move(code_config.udf_handler_name);
    code_object_metadata_.logical_commit_time = code_config.logical_commit_time;
    code_object_metadata_.version = code_config.version;
    PS_VLOG(5, log_context)
        << "Successfully set UDF code object with handler_name "
        << code_object_metadata_.handler_name;
    return absl::OkStatus();
  }

  absl::Status SetWasmCodeObject(
      CodeConfig code_config,
      privacy_sandbox::server_common::log::PSLogContext& log_context) {
    const auto code_object_status =
        SetCodeObject(std::move(code_config), log_context);
    if (!code_object_status.ok()) {
      return code_object_status;
    }
    return absl::OkStatus();
  }

 private:
  InvocationStrRequest<std::weak_ptr<RequestContext>> BuildInvocationRequest(
      const RequestContextFactory& request_context_factory,
      std::vector<std::string> input) const {
    absl::ReaderMutexLock lock(&mutex_);
    return {.id = kInvocationRequestId,
            .version_string = absl::StrCat("v", code_object_metadata_.version),
            .handler_name = code_object_metadata_.handler_name,
            .tags = {{std::string(kTimeoutDurationTag),
                      FormatDuration(udf_timeout_)}},
            .input = std::move(input),
            .min_log_level = absl::LogSeverity(udf_min_log_level_),
            .metadata = request_context_factory.GetWeakCopy()};
  }

  CodeObject BuildCodeObject(std::string js, std::string wasm,
                             int64_t version) {
    return {.id = kCodeObjectId,
            .version_string = absl::StrCat("v", version),
            .js = std::move(js),
            .wasm = std::move(wasm)};
  }

  // Mutex for code_object_metadata_;
  mutable absl::Mutex mutex_;
  CodeObjectMetadata code_object_metadata_ ABSL_GUARDED_BY(mutex_);
  const absl::Duration udf_timeout_;
  const absl::Duration udf_update_timeout_;
  int udf_min_log_level_;
  // Per b/299667930, RomaService has been extended to support metadata
  // storage as a side effect of RomaService::Execute(), making it no longer
  // const. However, UDFClient::ExecuteCode() remains logically const, so
  // RomaService is marked as mutable to allow usage within
  // UDFClient::ExecuteCode(). For concerns about mutable or go/totw/174,
  // RomaService is thread-safe, so losing the thread-safety of usage within a
  // const function is a lesser concern.
  mutable RomaService<std::weak_ptr<RequestContext>> roma_service_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<UdfClient>> UdfClient::Create(
    Config<std::weak_ptr<RequestContext>>&& config, absl::Duration udf_timeout,
    absl::Duration udf_update_timeout, int udf_min_log_level) {
  auto udf_client = std::make_unique<UdfClientImpl>(
      std::move(config), udf_timeout, udf_update_timeout, udf_min_log_level);
  const auto init_status = udf_client->Init();
  if (!init_status.ok()) {
    return init_status;
  }
  return udf_client;
}

}  // namespace kv_server
