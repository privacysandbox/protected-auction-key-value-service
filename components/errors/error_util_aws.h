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

namespace kv_server {

absl::StatusCode HttpResponseCodeToStatusCode(
    const Aws::Http::HttpResponseCode& response_code);

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
}  // namespace kv_server
#endif  // COMPONENTS_ERRORS_AWS_ERROR_UTIL_H_
