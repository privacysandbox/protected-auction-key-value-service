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
#include "components/util/sleepfor_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

using ::testing::Return;

class RetryTest : public ::testing::Test {
 protected:
  privacy_sandbox::server_common::log::NoOpContext log_context_;
};

TEST_F(RetryTest, RetryUntilOk) {
  testing::MockFunction<absl::StatusOr<int>()> func;
  EXPECT_CALL(func, Call)
      .Times(2)
      .WillOnce([] { return absl::InvalidArgumentError("whatever"); })
      .WillOnce([] { return 1; });
  MockUnstoppableSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(absl::Seconds(2)))
      .Times(1)
      .WillOnce(Return(true));
  absl::AnyInvocable<void(const absl::Status&, int) const>
      status_count_metric_callback = [](const absl::Status&, int) {
        // no-op
      };
  absl::StatusOr<int> v =
      RetryUntilOk(func.AsStdFunction(), "TestFunc",
                   status_count_metric_callback, log_context_, sleep_for);
  EXPECT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 1);
}

TEST_F(RetryTest, RetryUnilOkStatusOnly) {
  testing::MockFunction<absl::Status()> func;
  EXPECT_CALL(func, Call)
      .Times(2)
      .WillOnce([] { return absl::InvalidArgumentError("whatever"); })
      .WillOnce([] { return absl::OkStatus(); });
  MockUnstoppableSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(absl::Seconds(2)))
      .Times(1)
      .WillOnce(Return(true));
  absl::AnyInvocable<void(const absl::Status&, int) const>
      status_count_metric_callback = [](const absl::Status&, int) {
        // no-op
      };
  RetryUntilOk(func.AsStdFunction(), "TestFunc", status_count_metric_callback,
               log_context_, sleep_for);
}

TEST_F(RetryTest, RetryWithMaxFailsWhenExceedingMax) {
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
  absl::AnyInvocable<void(const absl::Status&, int) const>
      status_count_metric_callback = [](const absl::Status&, int) {
        // no-op
      };
  absl::StatusOr<int> v =
      RetryWithMax(func.AsStdFunction(), "TestFunc", 2,
                   status_count_metric_callback, sleep_for, log_context_);
  EXPECT_FALSE(v.ok());
  EXPECT_EQ(v.status(), absl::InvalidArgumentError("whatever"));
}

TEST_F(RetryTest, RetryWithMaxSucceedsOnMax) {
  testing::MockFunction<absl::StatusOr<int>()> func;
  EXPECT_CALL(func, Call)
      .Times(2)
      .WillOnce([] { return absl::InvalidArgumentError("whatever"); })
      .WillOnce([] { return 1; });

  MockSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(absl::Seconds(2)))
      .Times(1)
      .WillOnce(Return(true));
  absl::AnyInvocable<void(const absl::Status&, int) const>
      status_count_metric_callback = [](const absl::Status&, int) {
        // no-op
      };
  absl::StatusOr<int> v =
      RetryWithMax(func.AsStdFunction(), "TestFunc", 2,
                   status_count_metric_callback, sleep_for, log_context_);
  EXPECT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 1);
}

TEST_F(RetryTest, RetryWithMaxSucceedsEarly) {
  testing::MockFunction<absl::StatusOr<int>()> func;
  EXPECT_CALL(func, Call)
      .Times(2)
      .WillOnce([] { return absl::InvalidArgumentError("whatever"); })
      .WillOnce([] { return 1; });
  MockSleepFor sleep_for;
  EXPECT_CALL(sleep_for, Duration(absl::Seconds(2)))
      .Times(1)
      .WillOnce(Return(true));
  absl::AnyInvocable<void(const absl::Status&, int) const>
      status_count_metric_callback = [](const absl::Status&, int) {
        // no-op
      };
  absl::StatusOr<int> v =
      RetryWithMax(func.AsStdFunction(), "TestFunc", 300,
                   status_count_metric_callback, sleep_for, log_context_);
  EXPECT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 1);
}

}  // namespace
}  // namespace kv_server
