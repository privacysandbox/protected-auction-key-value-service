/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef COMPONENTS_ERRORS_AWS_ERROR_UTIL_H_
#define COMPONENTS_ERRORS_AWS_ERROR_UTIL_H_

#include <utility>

#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "aws/core/client/AWSError.h"

namespace fledge::kv_server {
namespace {  // NOLINT(build/namespaces_headers)
absl::StatusCode HttpResponseCodeToStatusCode(
    Aws::Http::HttpResponseCode response_code) {
  // https://sdk.amazonaws.com/cpp/api/0.12.9/d1/d33/_http_response_8h_source.html
  // https://github.com/googleapis/googleapis/blob/master/google/rpc/code.proto
  const int http_code = static_cast<int>(response_code);
  switch (http_code) {
    case 400:
      return absl::StatusCode::kInvalidArgument;
    case 401:
      return absl::StatusCode::kUnauthenticated;
    case 403:
      return absl::StatusCode::kPermissionDenied;
    case 404:
      return absl::StatusCode::kNotFound;
    case 408:
    case 440:
      return absl::StatusCode::kDeadlineExceeded;
    case 409:
      return absl::StatusCode::kAlreadyExists;
    case 412:
    case 427:
      return absl::StatusCode::kFailedPrecondition;
    case 429:
      return absl::StatusCode::kResourceExhausted;
    case 499:
      return absl::StatusCode::kCancelled;
    case 500:
      return absl::StatusCode::kInternal;
    case 501:
      return absl::StatusCode::kUnimplemented;
    case 503:
      return absl::StatusCode::kUnavailable;
    case 504:
    case 598:
    case 599:
      return absl::StatusCode::kDeadlineExceeded;
    default:
      if (http_code >= 200 && http_code < 300) {
        return absl::StatusCode::kOk;
      }
      if (http_code >= 400 && http_code < 500) {
        return absl::StatusCode::kFailedPrecondition;
      }
      if (http_code >= 500 && http_code < 600) {
        return absl::StatusCode::kInternal;
      }
      return absl::StatusCode::kUnknown;
  }
}
}  // namespace

template <class T>
absl::Status AwsErrorToStatus(const Aws::Client::AWSError<T>& error) {
  const auto status_code =
      HttpResponseCodeToStatusCode(error.GetResponseCode());
  auto status = absl::Status(status_code, error.GetMessage());
  if (!status.ok()) {
    for (auto [header, value] : error.GetResponseHeaders()) {
      status.SetPayload(header, absl::Cord(std::move(value)));
    }
  }
  return status;
}
}  // namespace fledge::kv_server
#endif  // COMPONENTS_ERRORS_AWS_ERROR_UTIL_H_
