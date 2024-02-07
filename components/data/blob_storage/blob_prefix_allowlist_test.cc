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

#include "components/data/blob_storage/blob_prefix_allowlist.h"

#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(BlobPrefixAllowlistTest, ValidateParsingBlobName) {
  auto result = ParseBlobName("DELTA_1705430864435450");
  EXPECT_EQ(result.key, "DELTA_1705430864435450");
  EXPECT_EQ(result.prefix, "");

  result = ParseBlobName("prefix/DELTA_1705430864435450");
  EXPECT_EQ(result.key, "DELTA_1705430864435450");
  EXPECT_EQ(result.prefix, "prefix");

  result = ParseBlobName("prefix1/prefix2/DELTA_1705430864435450");
  EXPECT_EQ(result.key, "DELTA_1705430864435450");
  EXPECT_EQ(result.prefix, "prefix1/prefix2");
}

TEST(BlobPrefixAllowlistTest, ValidateContainsPrefix) {
  auto allowlist = BlobPrefixAllowlist("prefix1,prefix1/prefix2");
  EXPECT_TRUE(allowlist.Contains(""));
  EXPECT_TRUE(allowlist.ContainsBlobPrefix("DELTA_1705430864435450"));
  EXPECT_TRUE(allowlist.Contains("prefix1"));
  EXPECT_TRUE(allowlist.ContainsBlobPrefix("prefix1/DELTA_1705430864435450"));
  EXPECT_TRUE(allowlist.Contains("prefix1/prefix2"));
  EXPECT_TRUE(
      allowlist.ContainsBlobPrefix("prefix1/prefix2/DELTA_1705430864435450"));
  EXPECT_FALSE(allowlist.Contains("non-existant"));
  EXPECT_FALSE(
      allowlist.ContainsBlobPrefix("non-existant/DELTA_1705430864435450"));
}

}  // namespace
}  // namespace kv_server
