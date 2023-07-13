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

#include "public/sharding/sharding_function.h"

#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(ShardingFunctionTest, VerifyAssigningKeysToShards) {
  ShardingFunction func("");
  EXPECT_EQ(5, func.GetShardNumForKey("key1", 7));
  EXPECT_EQ(6, func.GetShardNumForKey("key2", 7));
  EXPECT_EQ(1, func.GetShardNumForKey("key3", 7));
}

}  // namespace
}  // namespace kv_server
