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

#include "public/constants.h"

#include "absl/strings/str_cat.h"

namespace kv_server {
namespace {
template <FileType::Enum file_type>
std::string FileFormatRegex() {
  return absl::StrCat(FilePrefix<file_type>(), kFileComponentDelimiter,
                      R"(\d{)", kLogicalTimeDigits, "}");
}
}  // namespace

// TODO(b/241944346): Make kLogicalTimeDigits configurable if necessary.
std::string_view DeltaFileFormatRegex() {
  static const std::string* kRegex =
      new std::string(FileFormatRegex<FileType::DELTA>());
  return *kRegex;
}

const std::regex& SnapshotFileFormatRegex() {
  static const std::regex* const snapshot_regex =
      new std::regex(FileFormatRegex<FileType::SNAPSHOT>());
  return *snapshot_regex;
}

}  // namespace kv_server
