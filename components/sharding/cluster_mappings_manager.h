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

#ifndef COMPONENTS_SHARDING_CLUSTER_MAPPINGS_MANAGER_H_
#define COMPONENTS_SHARDING_CLUSTER_MAPPINGS_MANAGER_H_

#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_format.h"
#include "components/cloud_config/instance_client.h"
#include "components/data/common/thread_manager.h"
#include "components/data_server/server/parameter_fetcher.h"
#include "components/errors/retry.h"
#include "components/sharding/shard_manager.h"

namespace kv_server {
// Continously updates shard manager's cluster mappings every
// `update_interval_millis`.
// Example:
//  auto cluster_mappings_manager_ = ClusterMappingsManager::Create(
//       environment_, num_shards_, *instance_client_);
//  cluster_mappings_manager_->Start(*shard_manager_);
class ClusterMappingsManager {
 public:
  virtual ~ClusterMappingsManager() = default;

  ClusterMappingsManager(
      std::string environment, int32_t num_shards,
      InstanceClient& instance_client,
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::unique_ptr<SleepFor> sleep_for = std::make_unique<SleepFor>(),
      int32_t update_interval_millis = 1000);
  // Retreives cluster mappings for the given `environment`, which are
  // necessary for the ShardManager.
  // Mappings are:
  // {shard_num --> {replica's private ip address 1, ... },...}
  // {{0 -> {ip1, ip2}}, ....{num_shards-1}-> {ipN, ipN+1}}
  virtual std::vector<absl::flat_hash_set<std::string>>
  GetClusterMappings() = 0;
  absl::Status Start(ShardManager& shard_manager);
  absl::Status Stop();
  bool IsRunning() const;
  static std::unique_ptr<ClusterMappingsManager> Create(
      std::string environment, int32_t num_shards,
      InstanceClient& instance_client, ParameterFetcher& parameter_fetcher,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));

 protected:
  void Watch(ShardManager& shard_manager);
  privacy_sandbox::server_common::log::PSLogContext& GetLogContext() const;

  std::string environment_;
  int32_t num_shards_;
  InstanceClient& instance_client_;
  std::unique_ptr<ThreadManager> thread_manager_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
  std::unique_ptr<SleepFor> sleep_for_;
  int32_t update_interval_millis_;
};

}  // namespace kv_server
#endif  // COMPONENTS_SHARDING_CLUSTER_MAPPINGS_MANAGER_H_
