/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_ERRORS_ERROR_TAG_H_
#define COMPONENTS_ERRORS_ERROR_TAG_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"

namespace kv_server {

// Sets the payload for an absl::Status
// The payload key is the file_name extracted from file_path
// The payload value is the error_tag_enum
template <typename T>
inline absl::Status StatusWithErrorTag(absl::Status status,
                                       std::string_view file_path,
                                       T error_tag_enum) {
  std::vector<std::string> file_segments = absl::StrSplit(file_path, "/");
  status.SetPayload(file_segments.back(),
                    absl::Cord(absl::StrCat(error_tag_enum)));
  return status;
}

}  // namespace kv_server
#endif  // COMPONENTS_ERRORS_ERROR_TAG_H_
