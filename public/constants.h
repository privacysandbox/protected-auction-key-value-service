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

#ifndef PUBLIC_CONSTANTS_H_
#define PUBLIC_CONSTANTS_H_

#include <regex>
#include <string_view>

#include "public/base_types.pb.h"

namespace kv_server {

// File basenames of the given type should start with this prefix.
template <FileType::Enum file_type>
constexpr std::string_view FilePrefix() {
  static_assert(file_type != FileType::FILE_TYPE_UNSPECIFIED,
                "Please specify file type");
  return FileType::Enum_Name(file_type);
}

// Used to join components in a file name.
constexpr std::string_view kFileComponentDelimiter = "_";

// Number of digits in logical time in file basename.
constexpr int kLogicalTimeDigits = 16;

// "DELTA_\d{16}"
// The first component represents the file type.
//
// The second component represents Logical time which must
// contain 16 digits (To accommodate Unix timestamp with microsecond precision).
// At this time, there is no expectation on the content of the logical time,
// except that a larger number indicates a more recent file.
//
// '_' is used to join components.
//
// For example, "DELTA_1659978505000000"
std::string_view DeltaFileFormatRegex();

// In FLEDGE API, for a list of keys, they can be written as keys=key1,key2
// This requires the server code to parse and the delimiter is documented here.
constexpr char kQueryArgDelimiter = ',';

// Returns a compiled snapshot file name regex defined as follows:
//
// Compiled regex = "SNAPSHOT_\d{16}"
// Regex parts:
// - prefix = "SNAPSHOT"
// - component delimiter = "_"
// - suffix = a 16 digit number that represents logical time. A larger number
//            indicates a more recent snapshot.
const std::regex& SnapshotFileFormatRegex();

}  // namespace kv_server

#endif  // PUBLIC_CONSTANTS_H_
