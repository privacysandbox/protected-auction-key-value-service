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

#include "components/errors/error_util_gcp.h"

#include <string>

#include "absl/status/status.h"
#include "google/cloud/status.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

static const char status_message[] = "SomeMessage";

void CompareStatuses(::google::cloud::StatusCode gstatus,
                     absl::StatusCode abslStatus) {
  auto status = ::google::cloud::Status(gstatus, status_message);
  auto converted_status = GoogleErrorStatusToAbslStatus(status);
  EXPECT_EQ(abslStatus, converted_status.code());
  EXPECT_EQ(converted_status.message(), status_message);
}

TEST(ErrorUtilGcpTest, StatusConversion) {
  CompareStatuses(::google::cloud::StatusCode::kInvalidArgument,
                  absl::StatusCode::kInvalidArgument);
  CompareStatuses(::google::cloud::StatusCode::kCancelled,
                  absl::StatusCode::kCancelled);
  CompareStatuses(::google::cloud::StatusCode::kUnknown,
                  absl::StatusCode::kUnknown);
  CompareStatuses(::google::cloud::StatusCode::kDeadlineExceeded,
                  absl::StatusCode::kDeadlineExceeded);
  CompareStatuses(::google::cloud::StatusCode::kAlreadyExists,
                  absl::StatusCode::kAlreadyExists);
  CompareStatuses(::google::cloud::StatusCode::kPermissionDenied,
                  absl::StatusCode::kPermissionDenied);
  CompareStatuses(::google::cloud::StatusCode::kResourceExhausted,
                  absl::StatusCode::kResourceExhausted);
}

}  // namespace
}  // namespace kv_server
