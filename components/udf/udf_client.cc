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
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "components/errors/retry.h"
#include "components/internal_lookup/lookup_client.h"
#include "components/telemetry/telemetry.h"
#include "components/udf/get_values_hook.h"
#include "glog/logging.h"
#include "roma/config/src/config.h"
#include "roma/config/src/function_binding_object.h"
#include "roma/interface/roma.h"

namespace kv_server {

namespace {
using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::Execute;
using google::scp::roma::FunctionBindingObject;
using google::scp::roma::InvocationRequestStrInput;
using google::scp::roma::LoadCodeObj;
using google::scp::roma::ResponseObject;
using google::scp::roma::RomaInit;
using google::scp::roma::RomaStop;
using google::scp::roma::WasmDataType;

constexpr char kExecuteCodeSpan[] = "UdfClientExecuteCode";
constexpr char kUpdateCodeObjectSpan[] = "UdfUpdateCodeObject";

constexpr char kGetValuesHookJsName[] = "getValues";
constexpr absl::Duration kCallbackTimeout = absl::Seconds(1);
constexpr absl::Duration kCodeUpdateTimeout = absl::Seconds(1);

// Roma IDs and version numbers are required for execution.
// We do not currently make use of IDs or the code version number, set them to
// constants.
constexpr char kCodeObjectId[] = "id";
constexpr char kInvocationRequestId[] = "id";
constexpr int kVersionNum = 1;

class UdfClientImpl : public UdfClient {
 public:
  UdfClientImpl() = default;

  absl::StatusOr<std::string> ExecuteCode(std::vector<std::string> keys) const {
    auto span = GetTracer()->StartSpan(kExecuteCodeSpan);
    auto scope = opentelemetry::trace::Scope(span);

    absl::Status response_status;
    std::string result;
    absl::Notification notification;
    InvocationRequestStrInput invocation_request =
        BuildInvocationRequest(std::move(keys));
    const auto status =
        Execute(std::make_unique<InvocationRequestStrInput>(invocation_request),
                [&notification, &response_status, &result](
                    std::unique_ptr<absl::StatusOr<ResponseObject>> response) {
                  if (response->ok()) {
                    auto& code_response = **response;
                    result = std::move(code_response.resp);
                  } else {
                    response_status.Update(std::move(response->status()));
                  }
                  notification.Notify();
                });
    if (!status.ok()) {
      LOG(ERROR) << "Error executing UDF: " << status;
      return status;
    }

    notification.WaitForNotificationWithTimeout(kCallbackTimeout);
    if (!response_status.ok()) {
      LOG(ERROR) << "Error executing UDF: " << response_status;
      return response_status;
    }
    return result;
  }

  static absl::Status Init(const Config& config) { return RomaInit(config); }

  absl::Status Stop() { return RomaStop(); }

  absl::Status SetCodeObject(CodeConfig code_config) {
    auto span = GetTracer()->StartSpan(kUpdateCodeObjectSpan);
    auto scope = opentelemetry::trace::Scope(span);

    absl::Status response_status;
    absl::Notification notification;
    CodeObject code_object =
        BuildCodeObject(std::move(code_config.js), std::move(code_config.wasm));
    absl::Status load_status =
        LoadCodeObj(std::make_unique<CodeObject>(code_object),
                    [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                      if (!resp->ok()) {
                        response_status.Update(std::move(resp->status()));
                      }
                      notification.Notify();
                    });
    if (!load_status.ok()) {
      LOG(ERROR) << "Error setting UDF Code object: " << load_status;
      return load_status;
    }

    notification.WaitForNotificationWithTimeout(kCodeUpdateTimeout);
    if (!response_status.ok()) {
      LOG(ERROR) << "Error setting UDF Code object: " << response_status;
      return response_status;
    }
    handler_name_ = std::move(code_config.udf_handler_name);
    return absl::OkStatus();
  }

  absl::Status SetWasmCodeObject(CodeConfig code_config,
                                 WasmDataType wasm_return_type) {
    auto code_object_status = SetCodeObject(std::move(code_config));
    if (!code_object_status.ok()) {
      return code_object_status;
    }
    wasm_return_type_ = wasm_return_type;
    return absl::OkStatus();
  }

 private:
  InvocationRequestStrInput BuildInvocationRequest(
      std::vector<std::string> keys) const {
    return {.id = kInvocationRequestId,
            .version_num = kVersionNum,
            .handler_name = handler_name_,
            .wasm_return_type = wasm_return_type_,
            .input = std::move(keys)};
  }

  CodeObject BuildCodeObject(std::string js, std::string wasm) {
    return {.id = kCodeObjectId,
            .version_num = kVersionNum,
            .js = std::move(js),
            .wasm = std::move(wasm)};
  }

  std::string handler_name_;
  WasmDataType wasm_return_type_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<UdfClient>> UdfClient::Create(
    const Config& config) {
  const auto init_status = UdfClientImpl::Init(config);
  if (!init_status.ok()) {
    return init_status;
  }
  return std::make_unique<UdfClientImpl>();
}

Config UdfClient::ConfigWithGetValuesHook(GetValuesHook& get_values_hook,
                                          const int number_of_workers) {
  auto function_object = std::make_unique<
      FunctionBindingObject<std::string, std::vector<std::string>>>();
  function_object->function_name = kGetValuesHookJsName;
  // TODO(b/260874774): Investigate other options
  function_object->function =
      [&get_values_hook](
          std::tuple<std::vector<std::string>>& in) -> std::string {
    return get_values_hook(in);
  };

  Config config;
  config.RegisterFunctionBinding(std::move(function_object));
  config.NumberOfWorkers = number_of_workers;
  return config;
}

}  // namespace kv_server
