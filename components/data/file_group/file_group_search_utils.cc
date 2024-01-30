// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/data/file_group/file_group_search_utils.h"

#include <algorithm>
#include <vector>

#include "absl/strings/match.h"
#include "public/data_loading/filename_utils.h"
#include "src/cpp/util/status_macro/status_macros.h"

namespace kv_server {
namespace {

BlobStorageClient::ListOptions CreateBlobListOptions(
    const FileGroupFilter& filter) {
  BlobStorageClient::ListOptions options;
  if (!filter.start_after_basename.empty()) {
    options.start_after = filter.start_after_basename;
  }
  if (filter.file_type.has_value()) {
    options.prefix = FileType::Enum_Name(*filter.file_type);
  }
  return options;
}

// Assumes that filenames are lexicographically ordered.
absl::StatusOr<std::vector<FileGroup>> CreateFileGroupsFromBlobNames(
    const std::vector<std::string_view>& filenames) {
  std::vector<FileGroup> file_groups;
  if (filenames.empty()) {
    return file_groups;
  }
  FileGroup current_group;
  PS_RETURN_IF_ERROR(current_group.AddFile(filenames[0]));
  for (int32_t file_index = 1; file_index < filenames.size(); file_index++) {
    const auto& filename = filenames[file_index];
    if (!absl::StartsWith(filename, current_group.Basename())) {
      file_groups.push_back(current_group);
      current_group = FileGroup();
    }
    PS_RETURN_IF_ERROR(current_group.AddFile(filename));
  }
  file_groups.push_back(current_group);
  return file_groups;
}

}  // namespace

absl::StatusOr<std::optional<FileGroup>> FindMostRecentFileGroup(
    const BlobStorageClient::DataLocation& location,
    const FileGroupFilter& filter, BlobStorageClient& blob_client) {
  PS_ASSIGN_OR_RETURN(auto file_groups,
                      FindFileGroups(location, filter, blob_client));
  if (!file_groups.empty()) {
    return file_groups.back();
  }
  return std::nullopt;
}

absl::StatusOr<std::vector<FileGroup>> FindFileGroups(
    const BlobStorageClient::DataLocation& location,
    const FileGroupFilter& filter, BlobStorageClient& blob_client) {
  PS_ASSIGN_OR_RETURN(
      auto blob_names,
      blob_client.ListBlobs(location, CreateBlobListOptions(filter)));
  std::vector<std::string_view> supported_blob_names;
  std::copy_if(
      blob_names.begin(), blob_names.end(),
      std::back_inserter(supported_blob_names), [](std::string_view blob_name) {
        return IsDeltaFilename(blob_name) || IsSnapshotFilename(blob_name) ||
               IsFileGroupFileName(blob_name);
      });
  PS_ASSIGN_OR_RETURN(auto file_groups,
                      CreateFileGroupsFromBlobNames(supported_blob_names));
  if (!filter.status.has_value()) {
    return file_groups;
  }
  std::vector<FileGroup> result;
  std::copy_if(file_groups.begin(), file_groups.end(),
               std::back_inserter(result),
               [&filter](const FileGroup& file_group) {
                 return *filter.status == file_group.GetStatus();
               });
  return result;
}

}  // namespace kv_server
