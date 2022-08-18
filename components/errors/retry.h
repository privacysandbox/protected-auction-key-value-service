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

#ifndef COMPONENTS_ERRORS_RETRY_H_
#define COMPONENTS_ERRORS_RETRY_H_

#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "glog/logging.h"

namespace fledge::kv_server {

// Retry the function with exponential backoff until it succeeds.
absl::Duration ExponentialBackoffForRetry(uint32_t retries);

// You shouldn't need to instantiate this class.
// Use `RetryUntilOk` which creates one for you.
template <typename Func>
class RetryableUntilOk {
 public:
  RetryableUntilOk(Func&& f, std::string task_name)
      : func_(std::forward<Func>(f)), task_name_(std::move(task_name)) {}

  // absl::StatusOr<T> has a typedef `value_type`
  typename std::invoke_result_t<Func>::value_type operator()() {
    uint32_t retries = 0;
    while (true) {
      if (const auto s = func_(); s.ok()) {
        return s.value();
      }
      const absl::Duration backoff = ExponentialBackoffForRetry(retries);
      ++retries;
      LOG(WARNING) << "Retrying task: " << task_name_ << "; Attempt number "
                   << retries;
      // TODO(b/235082948): Inject a clock and mock for tests.
      absl::SleepFor(backoff);
    }
  }

 private:
  Func func_;
  std::string task_name_;
};

// Retries functors that return an absl::StatusOr<T> until they are `ok`.
// The value of type T is returned by this function.
template <typename Func>
typename std::invoke_result_t<RetryableUntilOk<Func>> RetryUntilOk(
    Func&& f, std::string task_name) {
  return RetryableUntilOk(std::forward<Func>(f), std::move(task_name))();
}

}  // namespace fledge::kv_server

#endif  // COMPONENTS_ERRORS_RETRY_H_
