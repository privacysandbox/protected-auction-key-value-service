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

#ifndef PUBLIC_SHARDING_SHARDING_FUNCTION_H_
#define PUBLIC_SHARDING_SHARDING_FUNCTION_H_

#include <string>
#include <string_view>

#include "pir/hashing/sha256_hash_family.h"

namespace kv_server {

// Sharding function to assign different keys to shard numbers within the range
// [0, `num_shards`).
class ShardingFunction {
 public:
  explicit ShardingFunction(std::string seed);
  int GetShardNumForKey(std::string_view key, int num_shards) const;

 private:
  distributed_point_functions::SHA256HashFunction hash_function_;
};

}  // namespace kv_server

#endif  // PUBLIC_SHARDING_SHARDING_FUNCTION_H_
