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
#include <utility>

#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace fledge::kv_server {
namespace {

TEST(RetryTest, RetryUntilOk) {
  testing::MockFunction<absl::StatusOr<int>()> func;
  EXPECT_CALL(func, Call)
      .Times(2)
      .WillOnce([] { return absl::InvalidArgumentError("whatever"); })
      .WillOnce([] { return 1; });

  absl::StatusOr<int> v = RetryUntilOk(func.AsStdFunction(), "TestFunc");
  EXPECT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 1);
}

TEST(RetryTest, RetryWithMaxFailsWhenExceedingMax) {
  testing::MockFunction<absl::StatusOr<int>()> func;
  EXPECT_CALL(func, Call).Times(2).WillRepeatedly([] {
    return absl::InvalidArgumentError("whatever");
  });

  absl::StatusOr<int> v = RetryWithMax(func.AsStdFunction(), "TestFunc", 2);
  EXPECT_FALSE(v.ok());
  EXPECT_EQ(v.status(), absl::InvalidArgumentError("whatever"));
}

TEST(RetryTest, RetryWithMaxSucceedsOnMax) {
  testing::MockFunction<absl::StatusOr<int>()> func;
  EXPECT_CALL(func, Call)
      .Times(2)
      .WillOnce([] { return absl::InvalidArgumentError("whatever"); })
      .WillOnce([] { return 1; });

  absl::StatusOr<int> v = RetryWithMax(func.AsStdFunction(), "TestFunc", 2);
  EXPECT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 1);
}

TEST(RetryTest, RetryWithMaxSucceedsEarly) {
  testing::MockFunction<absl::StatusOr<int>()> func;
  EXPECT_CALL(func, Call)
      .Times(2)
      .WillOnce([] { return absl::InvalidArgumentError("whatever"); })
      .WillOnce([] { return 1; });

  absl::StatusOr<int> v = RetryWithMax(func.AsStdFunction(), "TestFunc", 300);
  EXPECT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 1);
}
}  // namespace
}  // namespace fledge::kv_server
