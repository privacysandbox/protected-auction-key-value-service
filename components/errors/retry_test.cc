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
#include "gtest/gtest.h"

namespace fledge::kv_server {
namespace {

TEST(RetryTest, Retry) {
  int num_retries = 2;
  int expected = 0;
  int v = RetryUntilOk(
      [&num_retries, &expected]() -> absl::StatusOr<int> {
        if (num_retries != 0) {
          expected++;
          num_retries--;
          return absl::InvalidArgumentError("whatever");
        }
        return expected;
      },
      "TestFunc");
  ASSERT_EQ(v, expected);
}

}  // namespace
}  // namespace fledge::kv_server
