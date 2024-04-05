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

#ifndef COMPONENTS_DATA_FILE_GROUP_FILE_GROUP_SEARCH_UTILS_H_
#define COMPONENTS_DATA_FILE_GROUP_FILE_GROUP_SEARCH_UTILS_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/file_group/file_group.h"
#include "public/base_types.pb.h"

namespace kv_server {

struct FileGroupFilter {
  // Only return file groups with a basename more recent than this one.
  // Return all file groups when `start_after_basename.empty()`.
  std::string start_after_basename;

  // Only return file groups of this type.
  // Return all file groups when `!type.has_value()`.
  std::optional<FileType::Enum> file_type;

  // Only consider file groups with this upload status.
  // Return all file groups when `!upload_status.has_value()`.
  std::optional<FileGroup::FileStatus> status;
};

// Finds the most recent file group in blob storage that matches the `filter`
// criteria.
//
// Returns a descriptive error on failure.
absl::StatusOr<std::optional<FileGroup>> FindMostRecentFileGroup(
    const BlobStorageClient::DataLocation& location,
    const FileGroupFilter& filter, BlobStorageClient& blob_client);

// Finds all file groups in blob storage that meet the `filter` criteria.
// Returned file groups are lexicographically ordered by basename.
//
// Returns (when status is ok):
// - file group: when file group matching `filer` is found.
// - std::nullopt: when file group matching `filter` is not found.
// Returns a descriptive error on failure.
absl::StatusOr<std::vector<FileGroup>> FindFileGroups(
    const BlobStorageClient::DataLocation& location,
    const FileGroupFilter& filter, BlobStorageClient& blob_client);

}  // namespace kv_server

#endif  // COMPONENTS_DATA_FILE_GROUP_FILE_GROUP_SEARCH_UTILS_H_
