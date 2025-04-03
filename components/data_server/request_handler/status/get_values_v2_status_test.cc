// Copyright 2025 Google LLC
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

#include "components/data_server/request_handler/status/get_values_v2_status.h"

#include <string>

#include "absl/status/status.h"
#include "components/data_server/request_handler/status/status_tag.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(GetExternalStatusForV2, ReturnsOkIfNotV2FormatError) {
  auto status = absl::InvalidArgumentError("error");
  EXPECT_TRUE(GetExternalStatusForV2(status).ok());
}

TEST(GetExternalStatusForV2, ReturnsStatusIfV2FormatError) {
  auto status = V2RequestFormatErrorAsExternalHttpError(
      absl::InvalidArgumentError("error"));
  EXPECT_FALSE(GetExternalStatusForV2(status).ok());
}

}  // namespace
}  // namespace kv_server
