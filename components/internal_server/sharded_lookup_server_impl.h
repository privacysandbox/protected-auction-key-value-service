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

#ifndef COMPONENTS_INTERNAL_SERVER_SHARDED_LOOKUP_SERVER_IMPL_H_
#define COMPONENTS_INTERNAL_SERVER_SHARDED_LOOKUP_SERVER_IMPL_H_

#include <future>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "components/data_server/cache/cache.h"
#include "components/internal_server/lookup.grpc.pb.h"
#include "components/internal_server/remote_lookup_client.h"
#include "components/sharding/shard_manager.h"
#include "grpcpp/grpcpp.h"
#include "pir/hashing/sha256_hash_family.h"
#include "src/cpp/telemetry/metrics_recorder.h"
#include "src/cpp/telemetry/telemetry.h"

namespace kv_server {

// Implements the internal lookup service for the data store.
class ShardedLookupServiceImpl final
    : public kv_server::InternalLookupService::Service {
 public:
  ShardedLookupServiceImpl(
      privacy_sandbox::server_common::MetricsRecorder& metrics_recorder,
      const Cache& cache, const int32_t num_shards,
      const int32_t current_shard_num, ShardManager& shard_manager,
      // We 're currently going with a default empty string and not
      // allowing AdTechs to modify it.
      const std::string hashing_seed = "")
      : metrics_recorder_(metrics_recorder),
        cache_(cache),
        num_shards_(num_shards),
        current_shard_num_(current_shard_num),
        hashing_seed_(hashing_seed),
        hash_function_(
            distributed_point_functions::SHA256HashFunction(hashing_seed_)),
        shard_manager_(shard_manager) {
    CHECK_GT(num_shards, 1)
        << "num_shards for ShardedLookupServiceImpl must be > 1";
  }

  virtual ~ShardedLookupServiceImpl() = default;

  // Iterates over all keys specified in the `request` and assigns them to shard
  // buckets. Then for each bucket it queries the underlying data shard. At the
  // moment, for the shard number matching the current server shard number, the
  // logic will lookup data in its own cache. Eventually, this will change when
  // we have two types of servers: UDF and data servers. Then the responses are
  // combined and the result is returned. If any underlying request fails -- we
  // return an empty response and `Internal` error as the status for the gRPC
  // status code.
  grpc::Status InternalLookup(
      grpc::ServerContext* context,
      const kv_server::InternalLookupRequest* request,
      kv_server::InternalLookupResponse* response) override;

  grpc::Status InternalRunQuery(
      grpc::ServerContext* context,
      const kv_server::InternalRunQueryRequest* request,
      kv_server::InternalRunQueryResponse* response) override;

 private:
  // Keeps sharded keys and assosiated metdata.
  struct ShardLookupInput {
    // Keys that are being looked up.
    std::vector<std::string_view> keys;
    // A serialized `InternalLookupRequest` with the corresponding keys
    // from `keys`.
    std::string serialized_request;
    // Identifies by how many chars `keys` should be padded, so that
    // all requests add up to the same length.
    int32_t padding;
  };
  std::vector<ShardLookupInput> ShardKeys(
      const absl::flat_hash_set<std::string_view>& keys,
      bool lookup_sets) const;
  void ComputePadding(std::vector<ShardLookupInput>& sk) const;
  void SerializeShardedRequests(std::vector<ShardLookupInput>& lookup_inputs,
                                bool lookup_sets) const;
  std::vector<ShardLookupInput> BucketKeys(
      const absl::flat_hash_set<std::string_view>& keys) const;
  absl::Status ProcessShardedKeys(
      const absl::flat_hash_set<std::string_view>& keys,
      InternalLookupResponse& response) const;
  absl::StatusOr<InternalLookupResponse> GetLocalValues(
      const std::vector<std::string_view>& key_list) const;
  absl::StatusOr<
      std::vector<std::future<absl::StatusOr<InternalLookupResponse>>>>
  GetLookupFutures(const std::vector<ShardLookupInput>& shard_lookup_inputs,
                   std::function<absl::StatusOr<InternalLookupResponse>(
                       const std::vector<std::string_view>& key_list)>
                       get_local_future) const;
  absl::StatusOr<
      absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>>
  GetShardedKeyValueSet(
      const absl::flat_hash_set<std::string_view>& key_set) const;
  absl::StatusOr<InternalLookupResponse> GetLocalKeyValuesSet(
      const std::vector<std::string_view>& key_list) const;
  void CollectKeySets(
      absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>&
          key_sets,
      InternalLookupResponse& keysets_lookup_response) const;

  privacy_sandbox::server_common::MetricsRecorder& metrics_recorder_;
  const Cache& cache_;
  const int32_t num_shards_;
  const int32_t current_shard_num_;
  const std::string hashing_seed_;
  const distributed_point_functions::SHA256HashFunction hash_function_;
  const ShardManager& shard_manager_;
};

}  // namespace kv_server

#endif  // COMPONENTS_INTERNAL_SERVER_SHARDED_LOOKUP_SERVER_IMPL_H_
