/*
 * Copyright 2023 Google LLC
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
#ifndef COMPONENTS_ERRORS_ERROR_UTIL_GCP_H_
#define COMPONENTS_ERRORS_ERROR_UTIL_GCP_H_

#include <utility>

#include "absl/status/status.h"
#include "google/cloud/status.h"

namespace kv_server {
absl::Status GoogleErrorStatusToAbslStatus(
    const ::google::cloud::Status& error);

}  // namespace kv_server
#endif  // COMPONENTS_ERRORS_ERROR_UTIL_GCP_H_
