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

#include "components/data_server/server/lifecycle_heartbeat.h"

#include <string>
#include <utility>

#include "absl/log/log.h"
#include "components/errors/retry.h"

namespace kv_server {

namespace {

constexpr absl::string_view kLaunchHookParameterSuffix = "launch-hook";
constexpr absl::Duration kLifecycleHeartbeatFrequency = absl::Seconds(30);

class LifecycleHeartbeatImpl : public LifecycleHeartbeat {
 public:
  explicit LifecycleHeartbeatImpl(
      std::unique_ptr<PeriodicClosure> heartbeat,
      InstanceClient& instance_client,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : heartbeat_(std::move(heartbeat)),
        instance_client_(instance_client),
        log_context_(log_context) {}

  ~LifecycleHeartbeatImpl() {
    if (is_running_) {
      Finish();
    }
  }

  absl::Status Start(const ParameterFetcher& parameter_fetcher) override {
    if (is_running_) {
      return absl::FailedPreconditionError(
          "LifecycleHeartbeat already running");
    }
    launch_hook_name_ =
        parameter_fetcher.GetParameter(kLaunchHookParameterSuffix);
    PS_LOG(INFO, log_context_) << "Retrieved " << kLaunchHookParameterSuffix
                               << " parameter: " << launch_hook_name_;

    absl::Status status =
        heartbeat_->StartDelayed(kLifecycleHeartbeatFrequency, [this] {
          if (const absl::Status status =
                  instance_client_.RecordLifecycleHeartbeat(launch_hook_name_);
              !status.ok()) {
            PS_LOG(WARNING, log_context_)
                << "Failed to record lifecycle heartbeat: " << status;
          }
        });
    if (status.ok()) {
      is_running_ = true;
    }
    return status;
  }

  void Finish() override {
    if (!is_running_) {
      return;
    }
    is_running_ = false;
    TraceRetryUntilOk(
        [this] {
          return instance_client_.CompleteLifecycle(launch_hook_name_);
        },
        "CompleteLifecycle", LogStatusSafeMetricsFn<kCompleteLifecycleStatus>(),
        log_context_);
    PS_LOG(INFO, log_context_)
        << "Completed lifecycle hook " << launch_hook_name_;
  }

 private:
  std::unique_ptr<PeriodicClosure> heartbeat_;
  InstanceClient& instance_client_;
  std::string launch_hook_name_;
  bool is_running_ = false;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

}  // namespace

std::unique_ptr<LifecycleHeartbeat> LifecycleHeartbeat::Create(
    std::unique_ptr<PeriodicClosure> heartbeat, InstanceClient& instance_client,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<LifecycleHeartbeatImpl>(std::move(heartbeat),
                                                  instance_client, log_context);
}

std::unique_ptr<LifecycleHeartbeat> LifecycleHeartbeat::Create(
    InstanceClient& instance_client,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<LifecycleHeartbeatImpl>(PeriodicClosure::Create(),
                                                  instance_client, log_context);
}
}  // namespace kv_server
