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

#include "components/errors/retry.h"
#include "components/sharding/cluster_mappings_manager.h"

namespace kv_server {
class AwsClusterMappingsManager : public ClusterMappingsManager {
 public:
  AwsClusterMappingsManager(
      std::string environment, int32_t num_shards,
      InstanceClient& instance_client,
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::unique_ptr<SleepFor> sleep_for = std::make_unique<SleepFor>(),
      int32_t update_interval_millis = 1000)
      : ClusterMappingsManager(std::move(environment), num_shards,
                               instance_client, log_context),
        asg_regex_{std::regex(absl::StrCat("kv-server-", environment_,
                                           R"(-(\d+)-instance-asg)"))} {}

  std::vector<absl::flat_hash_set<std::string>> GetClusterMappings() override {
    absl::flat_hash_set<std::string> instance_group_names;
    for (int i = 0; i < num_shards_; i++) {
      instance_group_names.insert(
          absl::StrFormat("kv-server-%s-%d-instance-asg", environment_, i));
    }
    DescribeInstanceGroupInput describe_instance_group_input =
        AwsDescribeInstanceGroupInput{.instance_group_names =
                                          instance_group_names};
    auto& instance_client = instance_client_;
    auto instance_group_instances = TraceRetryUntilOk(
        [&instance_client, &describe_instance_group_input] {
          return instance_client.DescribeInstanceGroupInstances(
              describe_instance_group_input);
        },
        "DescribeInstanceGroupInstances",
        LogStatusSafeMetricsFn<kDescribeInstanceGroupInstancesStatus>(),
        GetLogContext());
    return GroupInstancesToClusterMappings(instance_group_instances);
  }

 private:
  absl::StatusOr<int32_t> GetShardNumberOffAsgName(std::string asg_name) const {
    std::smatch match_result;
    if (std::regex_match(asg_name, match_result, asg_regex_)) {
      int32_t shard_num;
      if (!absl::SimpleAtoi(std::string(match_result[1]), &shard_num)) {
        std::string error = absl::StrFormat("Failed converting %s to int32.",
                                            std::string(match_result[1]));
        return absl::InvalidArgumentError(error);
      }
      return shard_num;
    }
    return absl::InvalidArgumentError(absl::StrCat("Can't parse: ", asg_name));
  }

  absl::flat_hash_map<std::string, std::string> GetInstanceIdToIpMapping(
      const std::vector<InstanceInfo>& instance_group_instances) const {
    absl::flat_hash_set<std::string> instance_ids;
    for (const auto& instance : instance_group_instances) {
      if (instance.service_status != InstanceServiceStatus::kInService) {
        continue;
      }
      instance_ids.insert(instance.id);
    }

    auto& instance_client = instance_client_;
    std::vector<InstanceInfo> instances_detailed_info = TraceRetryUntilOk(
        [&instance_client, &instance_ids] {
          return instance_client.DescribeInstances(instance_ids);
        },
        "DescribeInstances", LogStatusSafeMetricsFn<kDescribeInstancesStatus>(),
        GetLogContext());

    absl::flat_hash_map<std::string, std::string> mapping;
    for (const auto& instance : instances_detailed_info) {
      mapping.emplace(instance.id, instance.private_ip_address);
    }
    return mapping;
  }

  std::vector<absl::flat_hash_set<std::string>> GroupInstancesToClusterMappings(
      std::vector<InstanceInfo>& instance_group_instances) const {
    auto id_to_ip = GetInstanceIdToIpMapping(instance_group_instances);
    std::vector<absl::flat_hash_set<std::string>> cluster_mappings(num_shards_);
    for (const auto& instance : instance_group_instances) {
      if (instance.service_status != InstanceServiceStatus::kInService) {
        continue;
      }
      auto shard_num_status = GetShardNumberOffAsgName(instance.instance_group);
      if (!shard_num_status.ok()) {
        continue;
      }
      int32_t shard_num = *shard_num_status;
      if (shard_num >= num_shards_) {
        continue;
      }

      const auto key_iter = id_to_ip.find(instance.id);
      if (key_iter == id_to_ip.end() || key_iter->second.empty()) {
        continue;
      }

      cluster_mappings[shard_num].insert(key_iter->second);
    }
    return cluster_mappings;
  }

  std::regex asg_regex_;
};

std::unique_ptr<ClusterMappingsManager> ClusterMappingsManager::Create(
    std::string environment, int32_t num_shards,
    InstanceClient& instance_client, ParameterFetcher& parameter_fetcher,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<AwsClusterMappingsManager>(
      environment, num_shards, instance_client, log_context);
}

}  // namespace kv_server
