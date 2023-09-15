// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/internal_server/lookup_supplier.h"

#include <memory>

#include "components/data_server/cache/cache.h"
#include "components/internal_server/local_lookup.h"
#include "components/internal_server/lookup.h"
#include "components/internal_server/sharded_lookup.h"
#include "components/sharding/shard_manager.h"
#include "glog/logging.h"
#include "src/cpp/telemetry/metrics_recorder.h"

namespace kv_server {

namespace {
using privacy_sandbox::server_common::MetricsRecorder;

class LookupSupplierImpl : public LookupSupplier {
 public:
  explicit LookupSupplierImpl(const Lookup& local_lookup, int num_shards,
                              int shard_num, const ShardManager& shard_manager,
                              MetricsRecorder& metrics_recorder,
                              const Cache& cache)
      : local_lookup_(local_lookup),
        num_shards_(num_shards),
        shard_num_(shard_num),
        shard_manager_(shard_manager),
        metrics_recorder_(metrics_recorder),
        cache_(cache) {}

  std::unique_ptr<Lookup> GetLookup() const override {
    if (num_shards_ > 1) {
      VLOG(9) << "Creating sharded lookup";
      return CreateShardedLookup(local_lookup_, num_shards_, shard_num_,
                                 shard_manager_, metrics_recorder_);
    }
    VLOG(9) << "Creating local lookup";
    return CreateLocalLookup(cache_, metrics_recorder_);
  }

 private:
  const Lookup& local_lookup_;
  const int32_t num_shards_;
  const int32_t shard_num_;
  const ShardManager& shard_manager_;
  MetricsRecorder& metrics_recorder_;
  const Cache& cache_;
};
}  // namespace

std::unique_ptr<LookupSupplier> LookupSupplier::Create(
    const Lookup& local_lookup, int num_shards, int shard_num,
    const ShardManager& shard_manager, MetricsRecorder& metrics_recorder,
    const Cache& cache) {
  return std::make_unique<LookupSupplierImpl>(local_lookup, num_shards,
                                              shard_num, shard_manager,
                                              metrics_recorder, cache);
}

}  // namespace kv_server
