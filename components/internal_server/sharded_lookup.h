/*
 * Copyright 2023 Google LLC
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

#ifndef COMPONENTS_INTERNAL_SERVER_SHARDED_LOOKUP_H_
#define COMPONENTS_INTERNAL_SERVER_SHARDED_LOOKUP_H_

#include <memory>
#include <string>

#include "components/internal_server/lookup.h"
#include "components/sharding/shard_manager.h"
#include "public/sharding/key_sharder.h"

namespace kv_server {

std::unique_ptr<Lookup> CreateShardedLookup(const Lookup& local_lookup,
                                            const int32_t num_shards,
                                            const int32_t current_shard_num,
                                            const ShardManager& shard_manager,
                                            KeySharder key_sharder,
                                            bool add_chaff = true);

}  // namespace kv_server

#endif  // COMPONENTS_INTERNAL_SERVER_SHARDED_LOOKUP_H_
