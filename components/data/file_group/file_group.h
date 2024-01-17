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

#ifndef COMPONENTS_DATA_FILE_GROUP_FILE_GROUP_H_
#define COMPONENTS_DATA_FILE_GROUP_FILE_GROUP_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "public/base_types.pb.h"

namespace kv_server {

// A "file group" is a group of related snapshot/delta files that share a common
// prefix and are treated as a single file for reading purposes by the KV
// server.
//
// Note that this class is not thread safe.
class FileGroup {
 public:
  enum class FileStatus : int8_t {
    // Means some files are not yet added to the group.
    kPending = 0,
    // Means all files are added to the group.
    kComplete,
  };

  // Returns the total number of files in the group (including files that are
  // not yet added to the group but are supposed to be).
  [[nodiscard]] int32_t Size() const;

  // Returns type of files contained by this group. Currently this can only be
  // either `FileType::DELTA` or `FileType::SNAPSHOT`.
  [[nodiscard]] FileType::Enum Type() const;

  [[nodiscard]] std::string_view Basename() const;

  // Returns a list of filenames that have been added to the group so far.
  [[nodiscard]] const absl::flat_hash_set<std::string>& Filenames() const;

  // Attempts to add `filename` to this file group.
  // Returns:
  // - absl::OKStatus: when filename is added successfully or already exists in
  // the group.
  // - absl::InvalidArgumentError: when `filename` does not belong to this file
  // group or is not supported for file groups.
  absl::Status AddFile(std::string_view filename);

  [[nodiscard]] FileStatus GetStatus() const;

 private:
  struct Metadata {
    std::string basename;
    int32_t group_size = 0;
    int64_t logical_commit_time = -1;
    FileType::Enum file_type = FileType::FILE_TYPE_UNSPECIFIED;
  };

  absl::StatusOr<Metadata> ParseMetadataFromFilename(std::string_view filename);
  absl::Status ValidateIncomingMetadata(const Metadata& existing_metadata,
                                        const Metadata& incoming_metadata);

  // Cache metadata so we don't have to repeatedly parse one of the existing
  // files to get the group's `file_type`, `logical_commit_time` and`group_size`
  Metadata cached_metadata_;
  absl::flat_hash_set<std::string> files_set_;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_FILE_GROUP_FILE_GROUP_H_
