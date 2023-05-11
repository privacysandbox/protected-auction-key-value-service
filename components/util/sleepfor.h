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

#ifndef COMPONENTS_UTIL_SLEEPFOR_H_
#define COMPONENTS_UTIL_SLEEPFOR_H_

#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"

namespace kv_server {

class SleepFor {
 public:
  virtual ~SleepFor() = default;

  // This function is thread safe and can be called multiple times.
  // Returns true if the it has waited the entire `Duration`.
  virtual bool Duration(absl::Duration d) const;

  // This function is not thread safe.  It can only be called once.
  // Once called, any blocked or subsequent calls to `Duration` will
  // return immediately.
  virtual absl::Status Stop();

 private:
  absl::Notification notification_;
};

// For Test Only
class UnstoppableSleepFor : public SleepFor {
  absl::Status Stop() override;
};

}  // namespace kv_server

#endif  // COMPONENTS_UTIL_SLEEPFOR_H_
