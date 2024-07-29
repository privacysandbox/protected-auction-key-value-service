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

#include <algorithm>
#include <utility>

namespace kv_server {

using privacy_sandbox::server_common::ExpiringFlag;

absl::Status ParameterNotifier::Stop() {
  absl::Status status = sleep_for_->Stop();
  status.Update(thread_manager_->Stop());
  return status;
}

bool ParameterNotifier::IsRunning() const {
  return thread_manager_->IsRunning();
}

absl::StatusOr<bool> ParameterNotifier::ShouldGetParameter(
    ExpiringFlag& expiring_flag) {
  if (!expiring_flag.Get()) {
    PS_VLOG(5, log_context_)
        << "Backup poll on parameter update " << parameter_name_;
    return true;
  }
  absl::StatusOr<std::string> notification =
      WaitForNotification(expiring_flag.GetTimeRemaining(),
                          [this]() { return thread_manager_->ShouldStop(); });

  if (absl::IsDeadlineExceeded(notification.status())) {
    // Deadline exceeded while waiting, trigger backup poll
    PS_VLOG(5, log_context_)
        << "Backup poll on parameter update " << parameter_name_;
    return true;
  }
  if (!notification.ok()) {
    return notification.status();
  }
  return true;
}

}  // namespace kv_server
