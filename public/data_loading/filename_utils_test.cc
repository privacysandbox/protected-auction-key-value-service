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

}  // namespace
}  // namespace kv_server
