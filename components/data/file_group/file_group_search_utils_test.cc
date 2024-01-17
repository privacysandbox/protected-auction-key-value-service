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

#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/common/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

using testing::_;
using testing::AllOf;
using testing::Field;
using testing::Return;
using testing::UnorderedElementsAre;

TEST(FileGroupSearchUtilsTest, ValidateFindingFileGroupsBlobClientError) {
  BlobStorageClient::DataLocation location{.bucket = "bucket"};
  MockBlobStorageClient blob_client;
  EXPECT_CALL(blob_client, ListBlobs(_, _))
      .Times(2)
      .WillRepeatedly(
          Return(absl::InvalidArgumentError("bucket does not exist.")));
  auto file_groups = FindFileGroups(location, FileGroupFilter{}, blob_client);
  EXPECT_FALSE(file_groups.ok()) << file_groups.status();
  EXPECT_EQ(file_groups.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(file_groups.status().message(), "bucket does not exist.");
  auto file_group =
      FindMostRecentFileGroup(location, FileGroupFilter{}, blob_client);
  EXPECT_FALSE(file_group.ok()) << file_group.status();
  EXPECT_EQ(file_group.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(file_group.status().message(), "bucket does not exist.");
}

TEST(FileGroupSearchUtilsTest,
     ValidateFindingFileGroupsBlobClientEmptyResponse) {
  FileGroupFilter filter{.file_type = FileType::SNAPSHOT};
  BlobStorageClient::DataLocation location{.bucket = "bucket"};
  MockBlobStorageClient blob_client;
  EXPECT_CALL(
      blob_client,
      ListBlobs(Field(&BlobStorageClient::DataLocation::bucket, "bucket"),
                Field(&BlobStorageClient::ListOptions::prefix, "SNAPSHOT")))
      .Times(2)
      .WillRepeatedly(Return(std::vector<std::string>{}));
  auto file_group = FindMostRecentFileGroup(location, filter, blob_client);
  EXPECT_TRUE(file_group.ok()) << file_group.status();
  EXPECT_FALSE(file_group->has_value());  // No file group found.
  auto file_groups = FindFileGroups(location, filter, blob_client);
  EXPECT_TRUE(file_groups.ok()) << file_groups.status();
  EXPECT_TRUE(file_groups->empty());
}

TEST(FileGroupSearchUtilsTest, ValidateFindingSnapshotFileGroups) {
  MockBlobStorageClient blob_client;
  EXPECT_CALL(
      blob_client,
      ListBlobs(Field(&BlobStorageClient::DataLocation::bucket, "bucket"),
                Field(&BlobStorageClient::ListOptions::prefix, "SNAPSHOT")))
      .WillOnce(Return(std::vector<std::string>{
          "SNAPSHOT_1705430864435450_00000_OF_000003",
          "SNAPSHOT_1705430864435450_00001_OF_000003",
          "SNAPSHOT_1705430864435450_00002_OF_000003",
          "SNAPSHOT_1705430864435451_00000_OF_000002"}));
  auto file_groups = FindFileGroups(
      BlobStorageClient::DataLocation{.bucket = "bucket"},
      FileGroupFilter{.file_type = FileType::SNAPSHOT}, blob_client);
  EXPECT_TRUE(file_groups.ok()) << file_groups.status();
  ASSERT_EQ(file_groups->size(), 2);
  // Verify first file group
  EXPECT_EQ((*file_groups)[0].Size(), 3);
  EXPECT_EQ((*file_groups)[0].Basename(), "SNAPSHOT_1705430864435450");
  EXPECT_EQ((*file_groups)[0].GetStatus(), FileGroup::FileStatus::kComplete);
  EXPECT_EQ((*file_groups)[0].Type(), FileType::SNAPSHOT);
  EXPECT_THAT(
      (*file_groups)[0].Filenames(),
      UnorderedElementsAre("SNAPSHOT_1705430864435450_00000_OF_000003",
                           "SNAPSHOT_1705430864435450_00001_OF_000003",
                           "SNAPSHOT_1705430864435450_00002_OF_000003"));
  // Verify second file group
  EXPECT_EQ((*file_groups)[1].Size(), 2);
  EXPECT_EQ((*file_groups)[1].Basename(), "SNAPSHOT_1705430864435451");
  EXPECT_EQ((*file_groups)[1].GetStatus(), FileGroup::FileStatus::kPending);
  EXPECT_EQ((*file_groups)[1].Type(), FileType::SNAPSHOT);
  EXPECT_THAT(
      (*file_groups)[1].Filenames(),
      UnorderedElementsAre("SNAPSHOT_1705430864435451_00000_OF_000002"));
}

TEST(FileGroupSearchUtilsTest, ValidateFindingMostRecentSnapshotFileGroups) {
  MockBlobStorageClient blob_client;
  EXPECT_CALL(
      blob_client,
      ListBlobs(
          Field(&BlobStorageClient::DataLocation::bucket, "bucket"),
          AllOf(Field(&BlobStorageClient::ListOptions::prefix, "SNAPSHOT"),
                Field(&BlobStorageClient::ListOptions::start_after,
                      "start_after"))))
      .Times(2)
      .WillRepeatedly(Return(std::vector<std::string>{
          "SNAPSHOT_1705430864435450_00000_OF_000003",
          "SNAPSHOT_1705430864435450_00001_OF_000003",
          "SNAPSHOT_1705430864435450_00002_OF_000003",
          "SNAPSHOT_1705430864435451_00000_OF_000002"}));
  auto file_group = FindMostRecentFileGroup(
      BlobStorageClient::DataLocation{.bucket = "bucket"},
      FileGroupFilter{.start_after_basename = "start_after",
                      .file_type = FileType::SNAPSHOT,
                      .status = FileGroup::FileStatus::kComplete},
      blob_client);
  ASSERT_TRUE(file_group.ok()) << file_group.status();
  EXPECT_EQ((*file_group)->Size(), 3);
  EXPECT_EQ((*file_group)->Basename(), "SNAPSHOT_1705430864435450");
  EXPECT_EQ((*file_group)->GetStatus(), FileGroup::FileStatus::kComplete);
  EXPECT_EQ((*file_group)->Type(), FileType::SNAPSHOT);
  EXPECT_THAT(
      (*file_group)->Filenames(),
      UnorderedElementsAre("SNAPSHOT_1705430864435450_00000_OF_000003",
                           "SNAPSHOT_1705430864435450_00001_OF_000003",
                           "SNAPSHOT_1705430864435450_00002_OF_000003"));
  file_group = FindMostRecentFileGroup(
      BlobStorageClient::DataLocation{.bucket = "bucket"},
      FileGroupFilter{.start_after_basename = "start_after",
                      .file_type = FileType::SNAPSHOT,
                      .status = FileGroup::FileStatus::kPending},
      blob_client);
  ASSERT_TRUE(file_group.ok()) << file_group.status();
  EXPECT_EQ((*file_group)->Size(), 2);
  EXPECT_EQ((*file_group)->Basename(), "SNAPSHOT_1705430864435451");
  EXPECT_EQ((*file_group)->GetStatus(), FileGroup::FileStatus::kPending);
  EXPECT_EQ((*file_group)->Type(), FileType::SNAPSHOT);
  EXPECT_THAT(
      (*file_group)->Filenames(),
      UnorderedElementsAre("SNAPSHOT_1705430864435451_00000_OF_000002"));
}

}  // namespace
}  // namespace kv_server
