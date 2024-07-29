/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_CLOUD_CONFIG_PARAMETER_UPDATE_PARAMETER_NOTIFIER_H_
#define COMPONENTS_CLOUD_CONFIG_PARAMETER_UPDATE_PARAMETER_NOTIFIER_H_

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "components/data/common/change_notifier.h"
#include "components/data/common/notifier_metadata.h"
#include "components/data/common/thread_manager.h"
#include "components/util/sleepfor.h"
#include "src/logger/request_context_logger.h"

namespace kv_server {

// The ParameterNotifier watches the cloud pubsub notification on the
// changes of the value for a given parameter. When a notification is received
// or the poll period deadline is reached, this class executes the get parameter
// value callback to retrieve the updated parameter value, and executes the
// apply parameter value callback to use the retrieved parameter to do some
// operation (e.g. update logging verbosity with the updated verbosity level
// parameter value)
class ParameterNotifier {
 public:
  explicit ParameterNotifier(
      std::unique_ptr<ChangeNotifier> notifier, std::string parameter_name,
      const absl::Duration poll_frequency, std::unique_ptr<SleepFor> sleep_for,
      privacy_sandbox::server_common::SteadyClock& clock,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : notifier_(std::move(notifier)),
        parameter_name_(std::move(parameter_name)),
        thread_manager_(
            ThreadManager::Create("Parameter Notifier " + parameter_name_)),
        poll_frequency_(poll_frequency),
        sleep_for_(std::move(sleep_for)),
        clock_(clock),
        log_context_(log_context) {}
  virtual ~ParameterNotifier() = default;
  // Starts watching the parameter updates. The ParamType is the data type of
  // the value for the given parameter name. The data type can be int, bool or
  // string etc, depending on how the callbacks are defined.
  template <typename ParamType>
  absl::Status Start(
      std::function<absl::StatusOr<ParamType>(std::string_view param_name)>
          get_param_callback,
      std::function<void(ParamType param_value)> apply_param_callback);

  // Blocks until `IsRunning` is False.
  virtual absl::Status Stop();

  // Returns False before calling `Start` or after `Stop` is
  // successful.
  virtual bool IsRunning() const;

  static absl::StatusOr<std::unique_ptr<ParameterNotifier>> Create(
      NotifierMetadata notifier_metadata, std::string parameter_name,
      const absl::Duration poll_frequency,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));

 private:
  template <typename ParamType>
  // Starts thread for watching the parameter updates
  void Watch(
      std::function<absl::StatusOr<ParamType>(std::string_view param_name)>
          get_param_callback,
      std::function<void(ParamType param_value)> apply_param_callback);
  // Gets notification from pubsub, returns error status or the notification
  // message
  absl::StatusOr<std::string> WaitForNotification(
      absl::Duration wait_duration,
      const std::function<bool()>& should_stop_callback);
  absl::StatusOr<bool> ShouldGetParameter(
      privacy_sandbox::server_common::ExpiringFlag& expiring_flag);
  std::unique_ptr<ChangeNotifier> notifier_;
  const std::string parameter_name_;
  std::unique_ptr<ThreadManager> thread_manager_;
  const absl::Duration poll_frequency_;
  std::unique_ptr<SleepFor> sleep_for_;
  privacy_sandbox::server_common::SteadyClock& clock_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

template <typename ParamType>
absl::Status ParameterNotifier::Start(
    std::function<absl::StatusOr<ParamType>(std::string_view)>
        get_param_callback,
    std::function<void(ParamType)> apply_param_callback) {
  return thread_manager_->Start(
      [this, get_param_callback = std::move(get_param_callback),
       apply_param_callback = std::move(apply_param_callback)]() {
        Watch(std::move(get_param_callback), std::move(apply_param_callback));
      });
}

template <typename ParamType>
void ParameterNotifier::Watch(
    std::function<absl::StatusOr<ParamType>(std::string_view)>
        get_param_callback,
    std::function<void(ParamType)> apply_param_callback) {
  PS_LOG(INFO, log_context_)
      << "Started to watch " << parameter_name_ << " parameter update";
  privacy_sandbox::server_common::ExpiringFlag expiring_flag(clock_);
  uint32_t sequential_failures = 0;
  while (!thread_manager_->ShouldStop()) {
    const absl::StatusOr<bool> should_get_parameter =
        ShouldGetParameter(expiring_flag);
    if (!should_get_parameter.ok()) {
      ++sequential_failures;
      const absl::Duration backoff_time =
          std::min(expiring_flag.GetTimeRemaining(),
                   ExponentialBackoffForRetry(sequential_failures));
      PS_LOG(ERROR, log_context_)
          << "Failed to get parameter update notifications: " << parameter_name_
          << ", " << should_get_parameter.status() << ".  Waiting for "
          << backoff_time;
      if (!sleep_for_->Duration(backoff_time)) {
        PS_LOG(ERROR, log_context_)
            << "Failed to sleep for " << backoff_time << ".  SleepFor invalid.";
      }
      continue;
    }
    sequential_failures = 0;
    if (!*should_get_parameter) {
      continue;
    }
    expiring_flag.Set(poll_frequency_);
    auto param_result = get_param_callback(parameter_name_);
    if (param_result.ok()) {
      apply_param_callback(std::move(*param_result));
      PS_VLOG(5, log_context_) << "Applied the callback on the parameter";
    } else {
      PS_LOG(ERROR, log_context_) << "Failed to get parameter value for "
                                  << parameter_name_ << param_result.status();
    }
  }
}

}  // namespace kv_server

#endif  // COMPONENTS_CLOUD_CONFIG_PARAMETER_UPDATE_PARAMETER_NOTIFIER_H_
