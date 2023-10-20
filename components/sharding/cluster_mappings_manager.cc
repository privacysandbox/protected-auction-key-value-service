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

#include "components/sharding/cluster_mappings_manager.h"

#include "components/errors/retry.h"

namespace kv_server {
ClusterMappingsManager::ClusterMappingsManager(
    std::string environment, int32_t num_shards,
    privacy_sandbox::server_common::MetricsRecorder& metrics_recorder,
    InstanceClient& instance_client, std::unique_ptr<SleepFor> sleep_for,
    int32_t update_interval_millis)
    : environment_{std::move(environment)},
      num_shards_{num_shards},
      metrics_recorder_{metrics_recorder},
      instance_client_{instance_client},
      thread_manager_(TheadManager::Create("Cluster mappings updater")),
      sleep_for_(std::move(sleep_for)),
      update_interval_millis_(update_interval_millis) {
  CHECK_GT(num_shards, 1) << "num_shards for ShardedLookup must be > 1";
}

absl::Status ClusterMappingsManager::Start(ShardManager& shard_manager) {
  return thread_manager_->Start(
      [this, &shard_manager]() { Watch(shard_manager); });
}

absl::Status ClusterMappingsManager::Stop() {
  absl::Status status = sleep_for_->Stop();
  status.Update(thread_manager_->Stop());
  return status;
}

bool ClusterMappingsManager::IsRunning() const {
  return thread_manager_->IsRunning();
}

void ClusterMappingsManager::Watch(ShardManager& shard_manager) {
  while (!thread_manager_->ShouldStop()) {
    sleep_for_->Duration(absl::Milliseconds(update_interval_millis_));
    shard_manager.InsertBatch(GetClusterMappings());
  }
}
}  // namespace kv_server
