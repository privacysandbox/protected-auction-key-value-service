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

#ifndef PUBLIC_SHARDING_KEY_SHARDER_H_
#define PUBLIC_SHARDING_KEY_SHARDER_H_

#include <optional>
#include <regex>
#include <string>
#include <string_view>

#include "public/sharding/sharding_function.h"

namespace kv_server {

// Shard structure holds sharding information.
struct Shard {
  // Shard number [0;num_shards)
  int shard_num;
  // Sharding key off which the shard number above was calculated.
  // Only set if different from the key. Default value, empty string, denotes
  // that the sharding key is not set.
  std::string sharding_key;
};

// Key sharder generates a shard number for a key. It might apply data
// locality logic when doing so.
class KeySharder {
 public:
  // Constructs a key sharder that would calculate a shard number.
  // If `shard_key_regex` is set, data locality logic is applied during the
  // calculation.
  explicit KeySharder(ShardingFunction sharding_function,
                      std::optional<std::regex> shard_key_regex = std::nullopt);
  // Get a shard number for the given key.
  // If `shard_key_regex` is set, data locality logic is applied during the
  // calculation. Specifically, it would apply the regex to the key specified in
  // `GetShardNumForKey`. If there is a match, that would be treated as the
  // sharding key. Otherwise, the key itself is treated as the sharding
  // key.
  Shard GetShardNumForKey(std::string_view key, int num_shards) const;

 private:
  ShardingFunction sharding_function_;
  std::optional<std::regex> shard_key_regex_;
};

}  // namespace kv_server

#endif  // PUBLIC_SHARDING_KEY_SHARDER_H_
