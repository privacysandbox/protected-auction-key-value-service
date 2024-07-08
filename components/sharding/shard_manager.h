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

#ifndef COMPONENTS_SHARDING_SHARD_MANAGER_H_
#define COMPONENTS_SHARDING_SHARD_MANAGER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "components/internal_server/remote_lookup_client.h"
#include "src/logger/request_context_logger.h"

namespace kv_server {
// This class is useful for testing ShardManager
class RandomGenerator {
 public:
  virtual ~RandomGenerator() = default;
  // Generate a random number in the interval [0,upper_bound)
  virtual int64_t Get(int64_t upper_bound) = 0;
};

// This class allows communication between a UDF server and data servers.
// A mapping from a shard number to a set of ip addresses should be inserted
// periodically. The class allows to retreive a RemoteLookupClient assigned to a
// random ip address from the provided pool. ShardManager is thread safe.
class ShardManager {
 public:
  virtual ~ShardManager() = default;
  // Insert the mapping of { shard number -> corresponding replicas' ip
  // adresseses }. An index of the vector is the shard number. The length of the
  // vector must be equal to the `num_shards`.
  virtual void InsertBatch(const std::vector<absl::flat_hash_set<std::string>>&
                               cluster_mappings) = 0;
  // Given the shard number, get a remote lookup client for one of the replicas
  // in the pool.
  virtual RemoteLookupClient* Get(int64_t shard_num) const = 0;
  static absl::StatusOr<std::unique_ptr<ShardManager>> Create(
      int32_t num_shards,
      privacy_sandbox::server_common::KeyFetcherManagerInterface&
          key_fetcher_manager,
      const std::vector<absl::flat_hash_set<std::string>>& cluster_mappings,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));
  static absl::StatusOr<std::unique_ptr<ShardManager>> Create(
      int32_t num_shards,
      const std::vector<absl::flat_hash_set<std::string>>& cluster_mappings,
      std::unique_ptr<RandomGenerator> random_generator,
      std::function<std::unique_ptr<RemoteLookupClient>(const std::string& ip)>
          client_factory,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));
};
}  // namespace kv_server
#endif  // COMPONENTS_SHARDING_SHARD_MANAGER_H_
