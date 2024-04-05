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

#include "components/data/file_group/file_group.h"

#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "public/constants.h"
#include "public/data_loading/filename_utils.h"
#include "src/util/status_macro/status_macros.h"

namespace kv_server {
namespace {

constexpr int32_t kFileTypeStrIndex = 0;
constexpr int32_t kLogicalCommitTimeStrIndex = 1;
constexpr int32_t kFileGroupFileIndexStrIndex = 2;
constexpr int32_t kFileGroupSizeStrIndex = 4;
constexpr int32_t kFileGroupFileNameNumComponents = 5;
constexpr int32_t kStandardDeltaOrSnapshotFileGroupSize = 1;

absl::StatusOr<int32_t> ToInt64(absl::string_view digits) {
  if (int64_t result; absl::SimpleAtoi(digits, &result)) {
    return result;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Input ", digits, " is not numeric."));
}

}  // namespace

absl::StatusOr<FileGroup::Metadata> FileGroup::ParseMetadataFromFilename(
    std::string_view filename) {
  Metadata metadata;
  std::vector<std::string> name_parts =
      absl::StrSplit(filename, kFileComponentDelimiter);
  FileType::Enum file_type;
  if (!FileType_Enum_Parse(name_parts[kFileTypeStrIndex], &file_type)) {
    return absl::InvalidArgumentError(
        absl::StrCat("File type: ", name_parts[kFileTypeStrIndex]));
  }
  PS_ASSIGN_OR_RETURN(auto logical_commit_time,
                      ToInt64(name_parts[kLogicalCommitTimeStrIndex]));
  metadata.file_type = file_type;
  metadata.logical_commit_time = logical_commit_time;
  metadata.basename =
      absl::StrCat(name_parts[kFileTypeStrIndex], kFileComponentDelimiter,
                   name_parts[kLogicalCommitTimeStrIndex]);
  if (name_parts.size() == kFileGroupFileNameNumComponents) {
    PS_ASSIGN_OR_RETURN(auto file_index,
                        ToInt64(name_parts[kFileGroupFileIndexStrIndex]));
    PS_ASSIGN_OR_RETURN(auto file_group_size,
                        ToInt64(name_parts[kFileGroupSizeStrIndex]));
    if (file_index >= file_group_size) {
      return absl::InvalidArgumentError(
          absl::StrCat(filename, " is invalid. index: ", file_index,
                       " must be < ", file_group_size));
    }
    metadata.group_size = file_group_size;
  } else {
    metadata.group_size = kStandardDeltaOrSnapshotFileGroupSize;
  }
  return metadata;
}

absl::Status FileGroup::ValidateIncomingMetadata(
    const Metadata& existing_metadata, const Metadata& incoming_metadata) {
  if (existing_metadata.group_size != incoming_metadata.group_size) {
    return absl::InvalidArgumentError(
        absl::StrCat("Wrong group size: ", incoming_metadata.group_size,
                     ", group size is: ", existing_metadata.group_size));
  }
  if (existing_metadata.logical_commit_time !=
      incoming_metadata.logical_commit_time) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Wrong logical commit time: ", incoming_metadata.logical_commit_time,
        ", group commit time is: ", existing_metadata.logical_commit_time));
  }
  if (existing_metadata.file_type != incoming_metadata.file_type) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Wrong file type: ", FileType_Enum_Name(incoming_metadata.file_type),
        ", group file type is: ",
        FileType_Enum_Name(existing_metadata.file_type)));
  }
  return absl::OkStatus();
}

int32_t FileGroup::Size() const { return cached_metadata_.group_size; }

FileType::Enum FileGroup::Type() const { return cached_metadata_.file_type; }

std::string_view FileGroup::Basename() const {
  return cached_metadata_.basename;
}

const absl::flat_hash_set<std::string>& FileGroup::Filenames() const {
  return files_set_;
}

FileGroup::FileStatus FileGroup::GetStatus() const {
  return !files_set_.empty() && Size() == files_set_.size()
             ? FileStatus::kComplete
             : FileStatus::kPending;
}

absl::Status FileGroup::AddFile(std::string_view filename) {
  if (GetStatus() == FileStatus::kComplete || files_set_.contains(filename)) {
    return absl::OkStatus();
  }
  if (!IsDeltaFilename(filename) && !IsSnapshotFilename(filename) &&
      !IsFileGroupFileName(filename)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "File name: ", filename, " is not supported for file groups."));
  }
  PS_ASSIGN_OR_RETURN(auto incoming_metadata,
                      ParseMetadataFromFilename(filename));
  if (cached_metadata_.file_type == FileType::FILE_TYPE_UNSPECIFIED) {
    cached_metadata_ = incoming_metadata;
  }
  PS_RETURN_IF_ERROR(
      ValidateIncomingMetadata(cached_metadata_, incoming_metadata));
  files_set_.emplace(filename);
  return absl::OkStatus();
}

}  // namespace kv_server
