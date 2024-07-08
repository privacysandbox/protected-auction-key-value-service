// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "components/sharding/shard_manager.h"

#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"

namespace kv_server {
namespace {

class RandomGeneratorImpl : public RandomGenerator {
 public:
  RandomGeneratorImpl() : generator_{rand_dev_()} {}

  int64_t Get(int64_t upper_bound) {
    std::uniform_int_distribution<int> distr(0, upper_bound - 1);
    return distr(generator_);
  }

 private:
  std::random_device rand_dev_;
  std::mt19937 generator_;
};

class ShardManagerImpl : public ShardManager {
 public:
  ShardManagerImpl(
      int32_t num_shards,
      std::function<std::unique_ptr<RemoteLookupClient>(const std::string& ip)>
          client_factory,
      std::unique_ptr<RandomGenerator> random_generator,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : num_shards_{num_shards},
        client_factory_{client_factory},
        random_generator_{std::move(random_generator)},
        log_context_(log_context) {}

  // taking in a set to exclude duplicates.
  // set doesn't have an O(1) lookup --> converting to vector.
  void InsertBatch(const std::vector<absl::flat_hash_set<std::string>>&
                       cluster_mappings) override {
    if (cluster_mappings.size() != num_shards_) {
      return;
    }
    std::vector<std::vector<std::string>> cluster_mappings_vector;
    absl::MutexLock lock(&mutex_);
    for (const auto& si : cluster_mappings) {
      std::vector<std::string> vc(si.begin(), si.end());
      for (const auto& ip : vc) {
        const auto key_iter = remote_lookup_clients_.find(ip);
        if (key_iter != remote_lookup_clients_.end()) {
          continue;
        }
        remote_lookup_clients_.insert({ip, client_factory_(ip)});
      }
      cluster_mappings_vector.emplace_back(std::move(vc));
    }
    cluster_mappings_ = cluster_mappings_vector;
  }

  RemoteLookupClient* Get(int64_t shard_num) const override {
    absl::ReaderMutexLock lock(&mutex_);
    if (shard_num < 0 || shard_num >= num_shards_ ||
        cluster_mappings_.size() != num_shards_) {
      return nullptr;
    }
    const auto& shard_replicas = cluster_mappings_[shard_num];
    if (shard_replicas.size() == 0) {
      return nullptr;
    }
    const auto replica_idx = random_generator_->Get(shard_replicas.size());
    const auto& ip_address = shard_replicas[replica_idx];
    const auto key_iter = remote_lookup_clients_.find(ip_address);
    if (key_iter == remote_lookup_clients_.end()) {
      return nullptr;
    } else {
      return key_iter->second.get();
    }
  }

 private:
  mutable absl::Mutex mutex_;
  // (idx) shard id -> set of ip_addresses
  std::vector<std::vector<std::string>> cluster_mappings_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, std::unique_ptr<RemoteLookupClient>>
      remote_lookup_clients_ ABSL_GUARDED_BY(mutex_);
  int32_t num_shards_;
  std::function<std::unique_ptr<RemoteLookupClient>(const std::string& ip)>
      client_factory_;
  std::unique_ptr<RandomGenerator> random_generator_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

absl::Status ValidateMapping(
    int32_t num_shards,
    const std::vector<absl::flat_hash_set<std::string>>& cluster_mappings) {
  if (num_shards < 2) {
    return absl::InvalidArgumentError("Should have at least 2 clusters.");
  }

  if (num_shards != cluster_mappings.size()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "`num_shards`(%d) does not match the size of `cluster_mappings` (%d)",
        num_shards, cluster_mappings.size()));
  }

  for (auto& set : cluster_mappings) {
    if (set.empty()) {
      return absl::InvalidArgumentError(
          "Should have at least 1 replica per cluster.");
    }
  }

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ShardManager>> ShardManager::Create(
    int32_t num_shards,
    privacy_sandbox::server_common::KeyFetcherManagerInterface&
        key_fetcher_manager,
    const std::vector<absl::flat_hash_set<std::string>>& cluster_mappings,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  auto validationStatus = ValidateMapping(num_shards, cluster_mappings);
  if (!validationStatus.ok()) {
    return validationStatus;
  }
  auto shard_manager = std::make_unique<ShardManagerImpl>(
      cluster_mappings.size(),
      [&key_fetcher_manager](const std::string& ip) {
        return RemoteLookupClient::Create(ip, key_fetcher_manager);
      },
      std::make_unique<RandomGeneratorImpl>(), log_context);
  shard_manager->InsertBatch(std::move(cluster_mappings));
  return shard_manager;
}

absl::StatusOr<std::unique_ptr<ShardManager>> ShardManager::Create(
    int32_t num_shards,
    const std::vector<absl::flat_hash_set<std::string>>& cluster_mappings,
    std::unique_ptr<RandomGenerator> random_generator,
    std::function<std::unique_ptr<RemoteLookupClient>(const std::string& ip)>
        client_factory,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  auto validationStatus = ValidateMapping(num_shards, cluster_mappings);
  if (!validationStatus.ok()) {
    return validationStatus;
  }
  auto shard_manager = std::make_unique<ShardManagerImpl>(
      cluster_mappings.size(), client_factory, std::move(random_generator),
      log_context);
  shard_manager->InsertBatch(std::move(cluster_mappings));
  return shard_manager;
}
}  // namespace kv_server
