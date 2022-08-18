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

#include "public/data_loading/filename_utils.h"

#include <cctype>
#include <regex>  // NOLINT

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "glog/logging.h"
#include "public/constants.h"

namespace fledge::kv_server {

// Right now we use simple logic to validate, as the format is simple and we
// hope this is faster than doing a regex match.
bool IsDeltaFilename(std::string_view basename) {
  static const std::regex re(DeltaFileFormatRegex().data(),
                             DeltaFileFormatRegex().size());
  return std::regex_match(basename.begin(), basename.end(), re);
}

absl::StatusOr<std::string> ToDeltaFileName(uint64_t logical_commit_time) {
  const std::string result = absl::StrFormat(
      "%s%s%0*d", FilePrefix<FileType::DELTA>(), kFileComponentDelimiter,
      kLogicalTimeDigits, logical_commit_time);
  if (!IsDeltaFilename(result)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Unable to build delta file name with logical commit time: ",
        logical_commit_time, " which makes a file name: ", result));
  }
  return result;
}

}  // namespace fledge::kv_server
