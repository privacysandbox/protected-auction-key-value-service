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

#include "components/util/sleepfor.h"

#include "absl/log/log.h"

namespace kv_server {

bool SleepFor::Duration(absl::Duration d) const {
  return !notification_.WaitForNotificationWithTimeout(d);
}

absl::Status SleepFor::Stop() {
  if (notification_.HasBeenNotified()) {
    return absl::FailedPreconditionError("Already stopped");
  }
  notification_.Notify();
  return absl::OkStatus();
}

absl::Status UnstoppableSleepFor::Stop() {
  return absl::UnimplementedError("Don't call, for test only");
}

}  // namespace kv_server
