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

#include <memory>

#include "components/data_server/cache/cache.h"
#include "components/internal_server/lookup.h"
#include "components/sharding/shard_manager.h"
#include "src/cpp/telemetry/metrics_recorder.h"
namespace kv_server {
class LookupSupplier {
 public:
  virtual ~LookupSupplier() = default;

  virtual std::unique_ptr<Lookup> GetLookup() const = 0;

  static std::unique_ptr<LookupSupplier> Create(
      const Lookup& local_lookup, int num_shards, int shard_num,
      const ShardManager& shard_manager,
      privacy_sandbox::server_common::MetricsRecorder& metrics_recorder,
      const Cache& cache);
};
}  // namespace kv_server
