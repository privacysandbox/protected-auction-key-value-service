/*
 * Copyright 2023 Google LLC
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

#ifndef TOOLS_REQUEST_SIMULATION_RATE_LIMITER_H_
#define TOOLS_REQUEST_SIMULATION_RATE_LIMITER_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "components/util/sleepfor.h"
#include "src/util/duration.h"
namespace kv_server {

// A simple permit-based rate limiter. The permits are refilled at given rate
// passed in the constructor. The fill rate can also be updated during runtime
class RateLimiter {
 public:
  RateLimiter(int64_t initial_permits, int64_t permits_per_second,
              privacy_sandbox::server_common::SteadyClock& clock,
              std::unique_ptr<SleepFor> sleep_for,
              const absl::Duration& timeout)
      : permits_fill_rate_(permits_per_second),
        last_refill_time_(clock),
        clock_(clock),
        sleep_for_(std::move(sleep_for)),
        timeout_(timeout) {
    permits_.store(initial_permits, std::memory_order_relaxed);
  }
  ~RateLimiter() = default;
  // Acquires a single permit, returns waiting duration or error message
  absl::StatusOr<absl::Duration> Acquire() ABSL_LOCKS_EXCLUDED(mu_);
  // Acquires a number of permits, returns waiting duration or error message
  absl::StatusOr<absl::Duration> Acquire(int permits) ABSL_LOCKS_EXCLUDED(mu_);
  // Sets the fill rate
  void SetFillRate(int64_t permits_per_second) ABSL_LOCKS_EXCLUDED(mu_);

  // RateLimiter is neither copyable nor movable.
  RateLimiter(const RateLimiter&) = delete;
  RateLimiter& operator=(const RateLimiter&) = delete;

 private:
  void RefillPermits() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  mutable absl::Mutex mu_;
  // Permits fill rate in permits per second
  mutable int64_t permits_fill_rate_ ABSL_GUARDED_BY(mu_);
  // Last refill time in nanoseconds
  mutable privacy_sandbox::server_common::Stopwatch last_refill_time_
      ABSL_GUARDED_BY(mu_);
  // Number of permits available
  mutable std::atomic<int64_t> permits_;
  privacy_sandbox::server_common::SteadyClock& clock_;
  std::unique_ptr<SleepFor> sleep_for_;
  // Timeout period for acquiring permits
  absl::Duration timeout_;
  friend class RateLimiterTestPeer;
};

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_RATE_LIMITER_H_
