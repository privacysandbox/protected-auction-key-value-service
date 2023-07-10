// Copyright 2023 Google LLC
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

#include "tools/request_simulation/rate_limiter.h"

#include <iostream>

namespace kv_server {

absl::StatusOr<absl::Duration> RateLimiter::Acquire() { return Acquire(1); }

absl::StatusOr<absl::Duration> RateLimiter::Acquire(int permits) {
  absl::MutexLock lock(&mu_);
  const auto start_time = clock_.Now();
  auto deadline = start_time + timeout_;
  while (permits_.load(std::memory_order_relaxed) - permits < 0) {
    sleep_for_.Duration(
        absl::Milliseconds(1000 * permits / permits_fill_rate_));
    RefillPermits();
    if (clock_.Now() >= deadline) {
      return absl::DeadlineExceededError("Acquire deadline exceeds");
    }
  }
  permits_.fetch_sub(permits, std::memory_order_relaxed) - permits;
  return clock_.Now() - start_time;
}

void RateLimiter::RefillPermits() {
  const auto elapsed_time_ns =
      ToChronoNanoseconds(last_refill_time_.GetElapsedTime());
  if (elapsed_time_ns <= std::chrono::nanoseconds::zero()) {
    return;
  }
  const int64_t permits_to_fill =
      (permits_fill_rate_ / 1e9) * elapsed_time_ns.count();
  permits_.fetch_add(permits_to_fill, std::memory_order_relaxed) +
      permits_to_fill;
  last_refill_time_.Reset();
}

void RateLimiter::SetFillRate(int64_t permits_per_second) {
  absl::MutexLock lock(&mu_);
  permits_fill_rate_ = permits_per_second;
}

}  // namespace kv_server
