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

#ifndef PUBLIC_DATA_LOADING_FILENAME_UTILS_H_
#define PUBLIC_DATA_LOADING_FILENAME_UTILS_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace kv_server {

// File name must be in the form that conforms to Regex in
// DeltaFileFormatRegex() in constants.h
bool IsDeltaFilename(std::string_view basename);

absl::StatusOr<std::string> ToDeltaFileName(uint64_t logical_commit_time);

// Returns true is `basename` is a valid snapshot filename.
// Valid snapshot filenames conform to the regex return by
// `SnapshotFileFormatRegex()` in constants.h
bool IsSnapshotFilename(std::string_view basename);

// Attempts to construct a valid snapshot filename from `logical_commit_time`.
//
// Returns absl::InvalidArgumentError if `logical_commit_time` cannot be used to
// construct a valid snapshot filename.
absl::StatusOr<std::string> ToSnapshotFileName(uint64_t logical_commit_time);

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_FILENAME_UTILS_H_
