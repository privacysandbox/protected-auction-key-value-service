// Copyright 2023 Google LLC
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

#include "components/errors/error_util_aws.h"

#include "absl/status/status.h"
#include "aws/core/client/AWSError.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(ErrorUtilAwsTest, HttpStatuses) {
  EXPECT_EQ(absl::StatusCode::kInvalidArgument,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(400)));
  EXPECT_EQ(absl::StatusCode::kUnauthenticated,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(401)));
  EXPECT_EQ(absl::StatusCode::kPermissionDenied,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(403)));
  EXPECT_EQ(absl::StatusCode::kNotFound,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(404)));
  EXPECT_EQ(absl::StatusCode::kDeadlineExceeded,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(408)));
  EXPECT_EQ(absl::StatusCode::kDeadlineExceeded,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(440)));
  EXPECT_EQ(absl::StatusCode::kAlreadyExists,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(409)));
  EXPECT_EQ(absl::StatusCode::kFailedPrecondition,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(412)));
  EXPECT_EQ(absl::StatusCode::kFailedPrecondition,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(427)));
  EXPECT_EQ(absl::StatusCode::kResourceExhausted,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(429)));
  EXPECT_EQ(absl::StatusCode::kCancelled,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(499)));
  EXPECT_EQ(absl::StatusCode::kInternal,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(500)));
  EXPECT_EQ(absl::StatusCode::kUnimplemented,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(501)));
  EXPECT_EQ(absl::StatusCode::kUnavailable,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(503)));
  EXPECT_EQ(absl::StatusCode::kDeadlineExceeded,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(504)));
  EXPECT_EQ(absl::StatusCode::kDeadlineExceeded,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(598)));
  EXPECT_EQ(absl::StatusCode::kDeadlineExceeded,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(599)));
  EXPECT_EQ(absl::StatusCode::kOk,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(200)));
  EXPECT_EQ(absl::StatusCode::kOk,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(200)));
  EXPECT_EQ(absl::StatusCode::kFailedPrecondition,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(413)));
  EXPECT_EQ(absl::StatusCode::kFailedPrecondition,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(498)));
  EXPECT_EQ(absl::StatusCode::kInternal,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(505)));
  EXPECT_EQ(absl::StatusCode::kInternal,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(590)));
  EXPECT_EQ(absl::StatusCode::kUnknown,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(600)));
  EXPECT_EQ(absl::StatusCode::kUnknown,
            HttpResponseCodeToStatusCode(
                static_cast<Aws::Http::HttpResponseCode>(-1)));
}

}  // namespace
}  // namespace kv_server
