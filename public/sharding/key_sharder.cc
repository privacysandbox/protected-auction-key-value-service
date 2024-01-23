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

namespace kv_server {

KeySharder::KeySharder(ShardingFunction sharding_function,
                       std::optional<std::regex> shard_key_regex)
    : sharding_function_(std::move(sharding_function)),
      shard_key_regex_(std::move(shard_key_regex)) {}

Shard KeySharder::GetShardNumForKey(std::string_view key,
                                    int num_shards) const {
  if (shard_key_regex_.has_value()) {
    std::smatch match_result;
    // regex doesn't support string_view yet, though there is a proposal
    auto key_string = std::string(key);
    if (std::regex_match(key_string, match_result, shard_key_regex_.value())) {
      // match result doesn't implicitly convert to std::string_view, and we
      // also need to return it, so that the caller can log it.
      std::string sharding_key = std::move(match_result[1]);
      return Shard{.shard_num = sharding_function_.GetShardNumForKey(
                       sharding_key, num_shards),
                   .sharding_key = std::move(sharding_key)};
    }
  }
  return Shard{.shard_num =
                   sharding_function_.GetShardNumForKey(key, num_shards)};
}

}  // namespace kv_server
