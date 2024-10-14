// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/cloud_config/parameter_update/parameter_notifier.h"

namespace kv_server {

using privacy_sandbox::server_common::SteadyClock;

absl::StatusOr<std::string> ParameterNotifier::WaitForNotification(
    absl::Duration wait_duration,
    const std::function<bool()>& should_stop_callback) {
  absl::StatusOr<std::vector<std::string>> changes =
      notifier_->GetNotifications(wait_duration, should_stop_callback);
  if (!changes.ok()) {
    return changes.status();
  }
  if ((*changes).empty()) {
    return absl::DataLossError("Empty message in the notification");
  }
  // return the last element
  PS_VLOG(5, log_context_) << "Received notification for parameter update";
  return std::string((*changes).back());
}

absl::StatusOr<std::unique_ptr<ParameterNotifier>> ParameterNotifier::Create(
    NotifierMetadata notifier_metadata, std::string parameter_name,
    const absl::Duration poll_frequency,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  auto cloud_notifier_metadata =
      std::get<AwsNotifierMetadata>(notifier_metadata);
  cloud_notifier_metadata.queue_prefix = "ParameterNotifier_";
  PS_ASSIGN_OR_RETURN(
      auto notifier,
      ChangeNotifier::Create(std::move(cloud_notifier_metadata), log_context));
  return std::make_unique<ParameterNotifier>(
      std::move(notifier), std::move(parameter_name), poll_frequency,
      std::make_unique<SleepFor>(), SteadyClock::RealClock(), log_context);
}

}  // namespace kv_server
