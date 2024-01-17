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

#include "components/data/file_group/file_group.h"

#include "include/gmock/gmock.h"
#include "include/gtest/gtest.h"

namespace kv_server {
namespace {

TEST(FileGroupTest, ValidateDefaultFileGroup) {
  FileGroup file_group;
  EXPECT_EQ(file_group.Type(), FileType::FILE_TYPE_UNSPECIFIED);
  EXPECT_EQ(file_group.Size(), 0);
  EXPECT_EQ(file_group.GetStatus(), FileGroup::FileStatus::kPending);
  EXPECT_EQ(file_group.Basename(), "");
  EXPECT_TRUE(file_group.Filenames().empty());
}

TEST(FileGroupTest, ValidateAddingFilesUntilGroupIsComplete) {
  auto basename = "SNAPSHOT_1705430864435450";
  auto filename0 = "SNAPSHOT_1705430864435450_00000_OF_000002";
  FileGroup file_group;
  auto status = file_group.AddFile(filename0);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_EQ(file_group.Basename(), basename);
  EXPECT_EQ(file_group.Size(), 2);
  EXPECT_EQ(file_group.Type(), FileType::SNAPSHOT);
  EXPECT_EQ(file_group.GetStatus(), FileGroup::FileStatus::kPending);
  EXPECT_THAT(file_group.Filenames(), testing::UnorderedElementsAre(filename0));
  // Add the remaining file
  auto filename1 = "SNAPSHOT_1705430864435450_00001_OF_000002";
  status = file_group.AddFile(filename1);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_EQ(file_group.Basename(), basename);
  EXPECT_EQ(file_group.Size(), 2);
  EXPECT_EQ(file_group.Type(), FileType::SNAPSHOT);
  EXPECT_EQ(file_group.GetStatus(), FileGroup::FileStatus::kComplete);
  EXPECT_THAT(file_group.Filenames(),
              testing::UnorderedElementsAre(filename0, filename1));
}

TEST(FileGroupTest, ValidateAddingSingleDeltaFile) {
  auto delta_file = "DELTA_1705430864435450";
  FileGroup file_group;
  auto status = file_group.AddFile(delta_file);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_EQ(file_group.Basename(), delta_file);
  EXPECT_EQ(file_group.Size(), 1);
  EXPECT_EQ(file_group.Type(), FileType::DELTA);
  EXPECT_EQ(file_group.GetStatus(), FileGroup::FileStatus::kComplete);
  EXPECT_THAT(file_group.Filenames(),
              testing::UnorderedElementsAre(delta_file));
}

TEST(FileGroupTest, ValidateAddingSingleSnapshotFile) {
  auto snapshot_file = "SNAPSHOT_1705430864435450";
  FileGroup file_group;
  auto status = file_group.AddFile(snapshot_file);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_EQ(file_group.Basename(), snapshot_file);
  EXPECT_EQ(file_group.Size(), 1);
  EXPECT_EQ(file_group.Type(), FileType::SNAPSHOT);
  EXPECT_EQ(file_group.GetStatus(), FileGroup::FileStatus::kComplete);
  EXPECT_THAT(file_group.Filenames(),
              testing::UnorderedElementsAre(snapshot_file));
}

TEST(FileGroupTest, ValidateAddingInvalidFilenames) {
  FileGroup file_group;
  auto status = file_group.AddFile("");
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  status = file_group.AddFile("UNKNOWN_1705430864435450_00000_OF_000010");
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  status = file_group.AddFile("SNAPSHOT_1705430864435450_00000");
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  status = file_group.AddFile("SNAPSHOT_1705430864435450_00000_OF");
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  status = file_group.AddFile("DELTA_1705430864435450_OF_000010");
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  status = file_group.AddFile("SNAPSHOT_1705430864435450_00010_OF_000010");
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FileGroupTest, ValidateAddingFilesWithMismatchingMetadata) {
  FileGroup file_group;
  auto status = file_group.AddFile("SNAPSHOT_1705430864435450_00000_OF_000010");
  EXPECT_TRUE(status.ok()) << status;
  // check logical commit time mismatch
  status = file_group.AddFile("SNAPSHOT_1705430000000000_00000_OF_000010");
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  // check file type mismatch
  status = file_group.AddFile("DELTA_1705430864435450_00001_OF_000010");
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  // check group size mismatch
  status = file_group.AddFile("SNAPSHOT_1705430864435450_00001_OF_000020");
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace kv_server
