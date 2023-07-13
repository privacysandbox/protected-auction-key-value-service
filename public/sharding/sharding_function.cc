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

namespace kv_server {

ShardingFunction::ShardingFunction(std::string seed)
    : hash_function_(std::move(seed)) {}

int ShardingFunction::GetShardNumForKey(std::string_view key,
                                        int num_shards) const {
  return hash_function_(key, num_shards);
}

}  // namespace kv_server
