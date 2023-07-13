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
#include "components/errors/retry.h"
#include "components/sharding/shard_manager.h"

namespace kv_server {
// Continously updates shard manager's cluster mappings every
// `update_interval_millis`.
// Example:
//  cluster_mappings_manager_ = std::make_unique<ClusterMappingsManager>(
//       environment_, num_shards_, *metrics_recorder_, *instance_client_);
//  cluster_mappings_manager_->Start(*shard_manager_);
class ClusterMappingsManager {
 public:
  ClusterMappingsManager(
      std::string environment, int32_t num_shards,
      privacy_sandbox::server_common::MetricsRecorder& metrics_recorder,
      InstanceClient& instance_client,
      std::unique_ptr<SleepFor> sleep_for = std::make_unique<SleepFor>(),
      int32_t update_interval_millis = 1000)
      : environment_{std::move(environment)},
        num_shards_{num_shards},
        metrics_recorder_{metrics_recorder},
        instance_client_{instance_client},
        asg_regex_{std::regex(absl::StrCat("kv-server-", environment_,
                                           R"(-(\d+)-instance-asg)"))},
        thread_manager_(TheadManager::Create("Cluster mappings updater")),
        sleep_for_(std::move(sleep_for)),
        update_interval_millis_(update_interval_millis) {
    CHECK_GT(num_shards, 1)
        << "num_shards for ShardedLookupServiceImpl must be > 1";
  }

  // Retreives cluster mappings for the given `environment`, which are
  // neceesary for the ShardManager.
  // Mappings are:
  // {shard_num --> {replica's private ip address 1, ... },...}
  // {{0 -> {ip1, ip2}}, ....{num_shards-1}-> {ipN, ipN+1}}
  std::vector<absl::flat_hash_set<std::string>> GetClusterMappings();
  absl::Status Start(ShardManager& shard_manager);
  absl::Status Stop();
  bool IsRunning() const;

 private:
  void Watch(ShardManager& shard_manager);
  absl::StatusOr<int32_t> GetShardNumberOffAsgName(std::string asg_name) const;
  std::vector<absl::flat_hash_set<std::string>> GroupInstancesToClusterMappings(
      std::vector<InstanceInfo>& instance_group_instances) const;
  absl::flat_hash_map<std::string, std::string> GetInstaceIdToIpMapping(
      const std::vector<InstanceInfo>& instance_group_instances) const;

  std::string environment_;
  int32_t num_shards_;
  privacy_sandbox::server_common::MetricsRecorder& metrics_recorder_;
  InstanceClient& instance_client_;
  std::regex asg_regex_;
  std::unique_ptr<TheadManager> thread_manager_;
  std::unique_ptr<SleepFor> sleep_for_;
  int32_t update_interval_millis_;
};

}  // namespace kv_server
#endif  // COMPONENTS_SHARDING_CLUSTER_MAPPINGS_MANAGER_H_
