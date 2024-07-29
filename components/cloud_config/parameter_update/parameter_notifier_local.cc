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
  sleep_for_->Duration(wait_duration);
  return absl::DeadlineExceededError(
      "Parameter pubsub notification"
      "does not support local platform");
}

absl::StatusOr<std::unique_ptr<ParameterNotifier>> ParameterNotifier::Create(
    NotifierMetadata notifier_metadata, std::string parameter_name,
    const absl::Duration poll_frequency,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<ParameterNotifier>(
      nullptr, std::move(parameter_name), poll_frequency,
      std::make_unique<SleepFor>(), SteadyClock::RealClock(), log_context);
}
}  // namespace kv_server
