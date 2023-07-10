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

#include <algorithm>
#include <thread>
#include <vector>

#include "components/util/sleepfor_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {

using privacy_sandbox::server_common::SimulatedSteadyClock;
using privacy_sandbox::server_common::SteadyTime;
using testing::_;
using testing::Return;

class RateLimiterTestPeer {
 public:
  RateLimiterTestPeer() = delete;
  static int64_t ReadCurrentPermits(const RateLimiter& r) {
    absl::MutexLock lock(&r.mu_);
    return r.permits_.load(std::memory_order_relaxed);
  }
  static int64_t ReadRefillRate(const RateLimiter& r) {
    absl::MutexLock lock(&r.mu_);
    return r.permits_fill_rate_;
  }
  static SteadyTime ReadLastRefillTime(const RateLimiter& r) {
    absl::MutexLock lock(&r.mu_);
    return r.last_refill_time_.GetStartTime();
  }
};
namespace {

class RateLimiterTest : public ::testing::Test {
 protected:
  SimulatedSteadyClock sim_clock_;
};

TEST_F(RateLimiterTest, TestRefill) {
  MockSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(_)).WillRepeatedly(Return(true));
  RateLimiter rate_limiter(1, 1, sim_clock_, sleep_for, absl::Seconds(1));
  rate_limiter.Acquire();
  sim_clock_.AdvanceTime(absl::Seconds(1));
  rate_limiter.Acquire();
  EXPECT_EQ(RateLimiterTestPeer::ReadCurrentPermits(rate_limiter), 0);

  rate_limiter.SetFillRate(5);
  sim_clock_.AdvanceTime(absl::Seconds(1));
  rate_limiter.Acquire();
  EXPECT_EQ(RateLimiterTestPeer::ReadCurrentPermits(rate_limiter), 4);
}

TEST_F(RateLimiterTest, TestAcquireMultiplePermits) {
  MockSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(_)).WillRepeatedly(Return(true));
  // No refill
  int permits_to_acquire = 5;
  RateLimiter rate_limiter(permits_to_acquire, 0, sim_clock_, sleep_for,
                           absl::Seconds(1));
  // Acquire all available permits
  rate_limiter.Acquire(permits_to_acquire);
  EXPECT_EQ(RateLimiterTestPeer::ReadCurrentPermits(rate_limiter), 0);
}

TEST_F(RateLimiterTest, TestLastRefillTimeUpdate) {
  MockSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(_)).WillRepeatedly(Return(true));
  RateLimiter rate_limiter(1, 1, sim_clock_, sleep_for, absl::Seconds(1));
  const auto initial_refill_time =
      RateLimiterTestPeer::ReadLastRefillTime(rate_limiter);
  // trigger refill
  sim_clock_.AdvanceTime(absl::Seconds(1));
  rate_limiter.Acquire(2);
  const auto last_refill_time =
      RateLimiterTestPeer::ReadLastRefillTime(rate_limiter);
  EXPECT_EQ(last_refill_time - initial_refill_time, absl::Seconds(1));
  sim_clock_.AdvanceTime(absl::Seconds(1));
  // trigger refill again
  rate_limiter.Acquire(1);
  const auto last_refill_time2 =
      RateLimiterTestPeer::ReadLastRefillTime(rate_limiter);
  EXPECT_EQ(last_refill_time2 - last_refill_time, absl::Seconds(1));
}

TEST_F(RateLimiterTest, TestPermitsFillRate) {
  MockSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(_)).WillRepeatedly(Return(true));

  RateLimiter rate_limiter(0, 100, sim_clock_, sleep_for, absl::Seconds(1));
  sim_clock_.AdvanceTime(absl::Seconds(2));
  rate_limiter.Acquire();
  EXPECT_EQ(RateLimiterTestPeer::ReadCurrentPermits(rate_limiter), 199);

  rate_limiter.SetFillRate(1000);
  sim_clock_.AdvanceTime(absl::Seconds(1));
  rate_limiter.Acquire(200);
  EXPECT_EQ(RateLimiterTestPeer::ReadCurrentPermits(rate_limiter), 999);
}

TEST_F(RateLimiterTest, TestPermitsFillRateMultipleCallers) {
  MockSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(_)).WillRepeatedly(Return(true));
  RateLimiter rate_limiter(0, 1000, sim_clock_, sleep_for, absl::Seconds(100));
  std::vector<std::thread> threads;
  absl::Notification start;
  sim_clock_.AdvanceTime(absl::Seconds(1));
  // trigger a refill
  EXPECT_TRUE(rate_limiter.Acquire().ok());
  // Next callers will not trigger a refill because permits count are enough
  for (int i = 0; i < std::min(49, (int)std::thread::hardware_concurrency());
       ++i) {
    threads.emplace_back([&rate_limiter, &start]() {
      start.WaitForNotification();
      EXPECT_TRUE(rate_limiter.Acquire().ok());
    });
  }
  start.Notify();
  for (auto& thread : threads) {
    thread.join();
  }
  EXPECT_GE(RateLimiterTestPeer::ReadCurrentPermits(rate_limiter), 950);
  EXPECT_LT(RateLimiterTestPeer::ReadCurrentPermits(rate_limiter), 1000);
}

TEST_F(RateLimiterTest,
       TestPermitsFillRateMultipleCallersOnlyOneCallerCanAcquirePermit) {
  MockSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(_)).WillRepeatedly(Return(true));
  RateLimiter rate_limiter(0, 1, sim_clock_, sleep_for, absl::Seconds(0));
  std::vector<std::thread> threads;
  absl::Notification start;
  sim_clock_.AdvanceTime(absl::Seconds(1));
  std::atomic<int> acquire_success_count = 0;
  for (int i = 0; i < std::min(50, (int)std::thread::hardware_concurrency());
       ++i) {
    threads.emplace_back(
        [&rate_limiter, &start, &acquire_success_count, this]() {
          start.WaitForNotification();
          if (rate_limiter.Acquire().ok()) {
            acquire_success_count.fetch_add(1, std::memory_order_relaxed);
          }
        });
  }
  start.Notify();
  for (auto& thread : threads) {
    thread.join();
  }
  EXPECT_EQ(RateLimiterTestPeer::ReadCurrentPermits(rate_limiter), 0);
  EXPECT_EQ(acquire_success_count, 1);
}

TEST_F(RateLimiterTest, TestAcquireTimeout) {
  MockSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(_)).WillRepeatedly(Return(true));
  RateLimiter rate_limiter(0, 1, sim_clock_, sleep_for, absl::Seconds(0));
  sim_clock_.AdvanceTime(absl::Seconds(10));
  EXPECT_FALSE(rate_limiter.Acquire(100).ok());
}

}  // namespace
}  // namespace kv_server
