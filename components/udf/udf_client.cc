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
#include "glog/logging.h"
#include "roma/config/src/config.h"
#include "roma/interface/roma.h"

namespace kv_server {

namespace {
using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::Execute;
using google::scp::roma::InvocationRequestStrInput;
using google::scp::roma::LoadCodeObj;
using google::scp::roma::ResponseObject;
using google::scp::roma::RomaInit;
using google::scp::roma::RomaStop;

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
    std::shared_ptr<absl::Status> response_status =
        std::make_shared<absl::Status>();
    std::shared_ptr<std::string> result = std::make_shared<std::string>();
    std::shared_ptr<absl::Notification> notification =
        std::make_shared<absl::Notification>();
    InvocationRequestStrInput invocation_request =
        BuildInvocationRequest(std::move(keys));
    VLOG(9) << "Executing UDF";
    const auto status =
        Execute(std::make_unique<InvocationRequestStrInput>(invocation_request),
                [notification, response_status, result](
                    std::unique_ptr<absl::StatusOr<ResponseObject>> response) {
                  if (response->ok()) {
                    auto& code_response = **response;
                    *result = std::move(code_response.resp);
                  } else {
                    response_status->Update(std::move(response->status()));
                  }
                  notification->Notify();
                });
    if (!status.ok()) {
      LOG(ERROR) << "Error sending UDF for execution: " << status;
      return status;
    }

    notification->WaitForNotificationWithTimeout(kCallbackTimeout);
    if (!notification->HasBeenNotified()) {
      return absl::InternalError("Timed out waiting for UDF result.");
    }
    if (!response_status->ok()) {
      LOG(ERROR) << "Error executing UDF: " << *response_status;
      return *response_status;
    }
    return *result;
  }

  static absl::Status Init(const Config& config) { return RomaInit(config); }

  absl::Status Stop() { return RomaStop(); }

  absl::Status SetCodeObject(CodeConfig code_config) {
    // Only update code if logical commit time is larger.
    if (logical_commit_time_ >= code_config.logical_commit_time) {
      VLOG(1) << "Not updating code object. logical_commit_time "
              << code_config.logical_commit_time
              << " too small, should be greater than " << logical_commit_time_;
      return absl::OkStatus();
    }
    std::shared_ptr<absl::Status> response_status =
        std::make_shared<absl::Status>();
    std::shared_ptr<absl::Notification> notification =
        std::make_shared<absl::Notification>();
    CodeObject code_object =
        BuildCodeObject(std::move(code_config.js), std::move(code_config.wasm),
                        code_config.version);
    absl::Status load_status =
        LoadCodeObj(std::make_unique<CodeObject>(code_object),
                    [notification, response_status](
                        std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                      if (!resp->ok()) {
                        response_status->Update(std::move(resp->status()));
                      }
                      notification->Notify();
                    });
    if (!load_status.ok()) {
      LOG(ERROR) << "Error setting UDF Code object: " << load_status;
      return load_status;
    }

    notification->WaitForNotificationWithTimeout(kCodeUpdateTimeout);
    if (!notification->HasBeenNotified()) {
      return absl::InternalError("Timed out setting UDF code object.");
    }
    if (!response_status->ok()) {
      LOG(ERROR) << "Error setting UDF Code object: " << *response_status;
      return *response_status;
    }
    handler_name_ = std::move(code_config.udf_handler_name);
    logical_commit_time_ = code_config.logical_commit_time;
    version_ = code_config.version;
    return absl::OkStatus();
  }

  absl::Status SetWasmCodeObject(CodeConfig code_config) {
    const auto code_object_status = SetCodeObject(std::move(code_config));
    if (!code_object_status.ok()) {
      return code_object_status;
    }
    return absl::OkStatus();
  }

 private:
  InvocationRequestStrInput BuildInvocationRequest(
      std::vector<std::string> keys) const {
    return {.id = kInvocationRequestId,
            .version_num = static_cast<uint64_t>(version_),
            .handler_name = handler_name_,
            .input = std::move(keys)};
  }

  CodeObject BuildCodeObject(std::string js, std::string wasm,
                             int64_t version) {
    return {.id = kCodeObjectId,
            .version_num = static_cast<uint64_t>(version),
            .js = std::move(js),
            .wasm = std::move(wasm)};
  }

  std::string handler_name_;
  int64_t logical_commit_time_ = -1;
  int64_t version_ = 1;
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

}  // namespace kv_server
