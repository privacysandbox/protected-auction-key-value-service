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

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(IsDeltaFilename, IsDeltaFilename) {
  EXPECT_FALSE(IsDeltaFilename(""));
  EXPECT_FALSE(IsDeltaFilename("DALTE_"));
  EXPECT_FALSE(IsDeltaFilename("DELTA_"));
  EXPECT_FALSE(IsDeltaFilename("DELTA_123"));
  EXPECT_FALSE(IsDeltaFilename("DELTA_123451234512345x"));
  EXPECT_FALSE(IsDeltaFilename("Delta_1234512345123451"));
  EXPECT_TRUE(IsDeltaFilename("DELTA_1234512345123451"));
}

TEST(ToDeltaFileName, ToDeltaFileName) {
  EXPECT_FALSE(ToDeltaFileName(-1).ok());
  EXPECT_FALSE(ToDeltaFileName(12345123451234512).ok());

  for (const uint64_t value : {0ll, 1ll, 123ll, 1234512345123451ll}) {
    const auto result = ToDeltaFileName(value);
    ASSERT_TRUE(result.ok()) << result.status();
  }
  EXPECT_EQ(ToDeltaFileName(0).value(), ("DELTA_0000000000000000"));
  EXPECT_EQ(ToDeltaFileName(1).value(), ("DELTA_0000000000000001"));
  EXPECT_EQ(ToDeltaFileName(123).value(), ("DELTA_0000000000000123"));
  EXPECT_EQ(ToDeltaFileName(1234512345123451).value(),
            ("DELTA_1234512345123451"));
}

TEST(SnapshotFilename, IsSnapshotFilename) {
  EXPECT_FALSE(IsSnapshotFilename(""));
  EXPECT_FALSE(IsSnapshotFilename("SNAPSHOT_"));
  EXPECT_FALSE(IsSnapshotFilename("SNAPSHOT"));
  EXPECT_FALSE(IsSnapshotFilename("SNAPSHOT_1234512345123451x"));
  EXPECT_FALSE(IsSnapshotFilename("DELTA_1234512345123451"));
  EXPECT_FALSE(IsSnapshotFilename("SNAPSHOT_12345123451234510"));
  EXPECT_FALSE(IsSnapshotFilename("Snapshot_1234512345123451"));
  EXPECT_TRUE(IsSnapshotFilename("SNAPSHOT_1234512345123451"));
}

TEST(SnapshotFilename, ToSnapshotFilename) {
  EXPECT_FALSE(ToSnapshotFileName(-1).ok());
  EXPECT_EQ(ToSnapshotFileName(-1).status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(ToSnapshotFileName(1).ok());
  EXPECT_EQ(ToSnapshotFileName(1).value(), ("SNAPSHOT_0000000000000001"));
  EXPECT_TRUE(ToSnapshotFileName(1234512345123451).ok());
  EXPECT_EQ(ToSnapshotFileName(1234512345123451).value(),
            ("SNAPSHOT_1234512345123451"));
}

TEST(SnapshotFilename, IsLogicalShardingConfigFilename) {
  EXPECT_FALSE(IsLogicalShardingConfigFilename(""));
  EXPECT_FALSE(IsLogicalShardingConfigFilename("LOGICAL_SHARDING_CONFIG_"));
  EXPECT_FALSE(IsLogicalShardingConfigFilename("LOGICAL_SHARDING_CONFIG"));
  EXPECT_FALSE(IsLogicalShardingConfigFilename(
      "LOGICAL_SHARDING_CONFIG_1234512345123451x"));
  EXPECT_FALSE(IsLogicalShardingConfigFilename("DELTA_1234512345123451"));
  EXPECT_FALSE(IsLogicalShardingConfigFilename("SNAPSHOT_1234512345123451"));
  EXPECT_FALSE(IsLogicalShardingConfigFilename(
      "LOGICAL_SHARDING_CONFIG_12345123451234510"));
  EXPECT_FALSE(IsLogicalShardingConfigFilename(
      "Logical_sharding_config_1234512345123451"));
  EXPECT_TRUE(IsLogicalShardingConfigFilename(
      "LOGICAL_SHARDING_CONFIG_1234512345123451"));
}

TEST(SnapshotFilename, ToLogicalShardingConfigFilename) {
  EXPECT_FALSE(ToLogicalShardingConfigFilename(-1).ok());
  EXPECT_EQ(ToLogicalShardingConfigFilename(-1).status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(ToLogicalShardingConfigFilename(1).ok());
  EXPECT_EQ(ToLogicalShardingConfigFilename(1).value(),
            ("LOGICAL_SHARDING_CONFIG_0000000000000001"));
  EXPECT_TRUE(ToLogicalShardingConfigFilename(1234512345123451).ok());
  EXPECT_EQ(ToLogicalShardingConfigFilename(1234512345123451).value(),
            ("LOGICAL_SHARDING_CONFIG_1234512345123451"));
}

TEST(FileGroupFilename, IsFileGroupFileName) {
  EXPECT_FALSE(IsFileGroupFileName(""));
  EXPECT_FALSE(IsFileGroupFileName("DELTA"));
  EXPECT_FALSE(IsFileGroupFileName("SNAPSHOT"));
  EXPECT_FALSE(IsFileGroupFileName("DELTA_1234512345123451"));
  EXPECT_FALSE(IsFileGroupFileName("SNAPSHOT_1234512345123451"));
  EXPECT_FALSE(IsFileGroupFileName("SNAPSHOT_1234512345123451_00000"));
  EXPECT_TRUE(IsFileGroupFileName("DELTA_1234512345123451_00000_OF_000100"));
  EXPECT_TRUE(IsFileGroupFileName("SNAPSHOT_1234512345123451_00000_OF_000100"));
}

TEST(FileGroupFilename, ToFileGroupFileNameInvalidInputs) {
  auto filename =
      ToFileGroupFileName(FileType::DELTA, /*logical_commit_time=*/10,
                          /*file_index=*/11, /*file_group_size=*/10);
  EXPECT_FALSE(filename.ok()) << filename.status();
  EXPECT_THAT(filename.status().code(), absl::StatusCode::kInvalidArgument);
  filename = ToFileGroupFileName(FileType::FILE_TYPE_UNSPECIFIED,
                                 /*logical_commit_time=*/10,
                                 /*file_index=*/1, /*file_group_size=*/10);
  EXPECT_FALSE(filename.ok()) << filename.status();
  EXPECT_THAT(filename.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(FileGroupFilename, ToFileGroupFileNameValidInputs) {
  auto filename =
      ToFileGroupFileName(FileType::DELTA, /*logical_commit_time=*/10,
                          /*file_index=*/1, /*file_group_size=*/10);
  ASSERT_TRUE(filename.ok()) << filename.status();
  EXPECT_THAT(*filename, "DELTA_0000000000000010_00001_OF_000010");
  filename = ToFileGroupFileName(FileType::SNAPSHOT, /*logical_commit_time=*/10,
                                 /*file_index=*/0, /*file_group_size=*/10);
  ASSERT_TRUE(filename.ok()) << filename.status();
  EXPECT_THAT(*filename, "SNAPSHOT_0000000000000010_00000_OF_000010");
}

}  // namespace
}  // namespace kv_server
