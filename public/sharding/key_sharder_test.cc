/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "public/sharding/key_sharder.h"

#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(KeySharderTest, VerifyAssigningKeysToShards) {
  ShardingFunction func("");
  KeySharder key_sharder(func);
  EXPECT_EQ(5, key_sharder.GetShardNumForKey("key1", 7).shard_num);
  EXPECT_EQ(6, key_sharder.GetShardNumForKey("key2", 7).shard_num);
  EXPECT_EQ(1, key_sharder.GetShardNumForKey("key3", 7).shard_num);
}

TEST(KeySharderTest, VerifyAssigningKeysToShardsWithRegex) {
  ShardingFunction func("");
  KeySharder key_sharder(func, std::regex("(.*)_.*"));
  auto result = key_sharder.GetShardNumForKey("key1_blah", 7);
  EXPECT_EQ(5, result.shard_num);
  EXPECT_EQ("key1", result.sharding_key);
  result = key_sharder.GetShardNumForKey("key1_blahblah", 7);
  EXPECT_EQ(5, result.shard_num);
  EXPECT_EQ("key1", result.sharding_key);
  result = key_sharder.GetShardNumForKey("key1_blahblahblah", 7);
  EXPECT_EQ(5, result.shard_num);
  EXPECT_EQ("key1", result.sharding_key);
  // no match -- no problem
  EXPECT_EQ(5, key_sharder.GetShardNumForKey("key1", 7).shard_num);
  EXPECT_EQ(6, key_sharder.GetShardNumForKey("key2", 7).shard_num);
  EXPECT_EQ(1, key_sharder.GetShardNumForKey("key3", 7).shard_num);
}

// try with regex which doesn't match

}  // namespace
}  // namespace kv_server
