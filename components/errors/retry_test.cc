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

#include "components/errors/retry.h"

#include <string>
#include <thread>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/errors/mocks.h"
#include "components/telemetry/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

using ::testing::Return;

TEST(RetryTest, RetryUntilOk) {
  testing::MockFunction<absl::StatusOr<int>()> func;
  EXPECT_CALL(func, Call)
      .Times(2)
      .WillOnce([] { return absl::InvalidArgumentError("whatever"); })
      .WillOnce([] { return 1; });
  MockUnstoppableSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(absl::Seconds(2)))
      .Times(1)
      .WillOnce(Return(true));
  MockMetricsRecorder metrics_recorder;
  EXPECT_CALL(metrics_recorder,
              IncrementEventStatus("TestFunc",
                                   absl::InvalidArgumentError("whatever"), 1));
  EXPECT_CALL(metrics_recorder,
              IncrementEventStatus("TestFunc", absl::OkStatus(), 1));
  absl::StatusOr<int> v = RetryUntilOk(func.AsStdFunction(), "TestFunc",
                                       &metrics_recorder, sleep_for);
  EXPECT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 1);
}

TEST(RetryTest, RetryUnilOkStatusOnly) {
  testing::MockFunction<absl::Status()> func;
  EXPECT_CALL(func, Call)
      .Times(2)
      .WillOnce([] { return absl::InvalidArgumentError("whatever"); })
      .WillOnce([] { return absl::OkStatus(); });
  MockUnstoppableSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(absl::Seconds(2)))
      .Times(1)
      .WillOnce(Return(true));
  MockMetricsRecorder metrics_recorder;
  EXPECT_CALL(metrics_recorder,
              IncrementEventStatus("TestFunc",
                                   absl::InvalidArgumentError("whatever"), 1));
  EXPECT_CALL(metrics_recorder,
              IncrementEventStatus("TestFunc", absl::OkStatus(), 1));
  RetryUntilOk(func.AsStdFunction(), "TestFunc", &metrics_recorder, sleep_for);
}

TEST(RetryTest, RetryWithMaxFailsWhenExceedingMax) {
  testing::MockFunction<absl::StatusOr<int>()> func;
  EXPECT_CALL(func, Call).Times(2).WillRepeatedly([] {
    return absl::InvalidArgumentError("whatever");
  });

  MockSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(absl::Seconds(2)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(sleep_for, Duration(absl::Seconds(4)))
      .Times(1)
      .WillOnce(Return(true));
  MockMetricsRecorder metrics_recorder;
  EXPECT_CALL(metrics_recorder,
              IncrementEventStatus("TestFunc",
                                   absl::InvalidArgumentError("whatever"), 1))
      .Times(2);
  absl::StatusOr<int> v = RetryWithMax(func.AsStdFunction(), "TestFunc", 2,
                                       &metrics_recorder, sleep_for);
  EXPECT_FALSE(v.ok());
  EXPECT_EQ(v.status(), absl::InvalidArgumentError("whatever"));
}

TEST(RetryTest, RetryWithMaxSucceedsOnMax) {
  testing::MockFunction<absl::StatusOr<int>()> func;
  EXPECT_CALL(func, Call)
      .Times(2)
      .WillOnce([] { return absl::InvalidArgumentError("whatever"); })
      .WillOnce([] { return 1; });

  MockSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(absl::Seconds(2)))
      .Times(1)
      .WillOnce(Return(true));
  MockMetricsRecorder metrics_recorder;
  EXPECT_CALL(metrics_recorder,
              IncrementEventStatus("TestFunc",
                                   absl::InvalidArgumentError("whatever"), 1));
  EXPECT_CALL(metrics_recorder,
              IncrementEventStatus("TestFunc", absl::OkStatus(), 1));
  absl::StatusOr<int> v = RetryWithMax(func.AsStdFunction(), "TestFunc", 2,
                                       &metrics_recorder, sleep_for);
  EXPECT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 1);
}

TEST(RetryTest, RetryWithMaxSucceedsEarly) {
  testing::MockFunction<absl::StatusOr<int>()> func;
  EXPECT_CALL(func, Call)
      .Times(2)
      .WillOnce([] { return absl::InvalidArgumentError("whatever"); })
      .WillOnce([] { return 1; });
  MockSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(absl::Seconds(2)))
      .Times(1)
      .WillOnce(Return(true));
  MockMetricsRecorder metrics_recorder;
  EXPECT_CALL(metrics_recorder,
              IncrementEventStatus("TestFunc",
                                   absl::InvalidArgumentError("whatever"), 1));
  EXPECT_CALL(metrics_recorder,
              IncrementEventStatus("TestFunc", absl::OkStatus(), 1));
  absl::StatusOr<int> v = RetryWithMax(func.AsStdFunction(), "TestFunc", 300,
                                       &metrics_recorder, sleep_for);
  EXPECT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 1);
}

TEST(SleepForTest, DoesSleep) {
  SleepFor sleep_for;
  absl::Time start = absl::Now();
  sleep_for.Duration(absl::Seconds(2));
  absl::Duration total = absl::Now() - start;
  // Make sure it is close enough to intended sleep time.
  ASSERT_LT(total - absl::Seconds(2), absl::Milliseconds(20));
}

TEST(SleepForTest, DoesConcurrentSleep) {
  SleepFor sleep_for;
  absl::Duration d1;
  std::thread t1([&sleep_for, &d1]() {
    const auto start = absl::Now();
    sleep_for.Duration(absl::Seconds(2));
    d1 = absl::Now() - start;
  });
  absl::Duration d2;
  std::thread t2([&sleep_for, &d2]() {
    const auto start = absl::Now();
    sleep_for.Duration(absl::Seconds(2));
    d2 = absl::Now() - start;
  });
  t1.join();
  t2.join();
  // Make sure it is close enough to intended sleep time.
  ASSERT_LT(d1 - absl::Seconds(2), absl::Milliseconds(20));
  ASSERT_LT(d2 - absl::Seconds(2), absl::Milliseconds(20));
}

TEST(SleepForTest, DoesSleepStop) {
  SleepFor sleep_for;
  absl::Time start;
  std::thread t([&sleep_for, &start]() {
    start = absl::Now();
    sleep_for.Duration(absl::Minutes(10));
  });
  ASSERT_TRUE(sleep_for.Stop().ok());
  t.join();
  absl::Duration total = absl::Now() - start;
  // Make sure stop is fast enough
  ASSERT_LT(total, absl::Seconds(20));
  // Can't stop twice
  ASSERT_FALSE(sleep_for.Stop().ok());
}

}  // namespace
}  // namespace kv_server
