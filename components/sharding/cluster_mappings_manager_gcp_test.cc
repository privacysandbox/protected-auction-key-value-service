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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/data_server/server/mocks.h"
#include "components/internal_server/constants.h"
#include "components/sharding/cluster_mappings_manager.h"
#include "components/sharding/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "src/cpp/encryption/key_fetcher/src/fake_key_fetcher_manager.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

class ClusterMappingsGcpTest : public ::testing::Test {
 protected:
  void SetUp() override {
    privacy_sandbox::server_common::telemetry::TelemetryConfig config_proto;
    config_proto.set_mode(
        privacy_sandbox::server_common::telemetry::TelemetryConfig::PROD);
    KVServerContextMap(
        privacy_sandbox::server_common::telemetry::BuildDependentConfig(
            config_proto));
  }
};

TEST_F(ClusterMappingsGcpTest, RetrieveMappingsSuccessfully) {
  std::string environment = "testenv";
  std::string project_id = "some-project-id";
  int32_t num_shards = 4;
  auto instance_client = std::make_unique<MockInstanceClient>();
  EXPECT_CALL(*instance_client, DescribeInstanceGroupInstances(::testing::_))
      .WillOnce([&](DescribeInstanceGroupInput& input) {
        auto gcp_describe_instance_group_input =
            std::get_if<GcpDescribeInstanceGroupInput>(&input);
        EXPECT_EQ(gcp_describe_instance_group_input->project_id, project_id);
        InstanceInfo ii1 = {
            .service_status = InstanceServiceStatus::kInService,
            .private_ip_address = "ip1",
            .labels = {{"shard-num", "0"}, {"initialized", "initialized"}},
        };
        InstanceInfo ii2 = {
            .service_status = InstanceServiceStatus::kInService,
            .private_ip_address = "ip2",
            .labels = {{"shard-num", "0"}, {"initialized", "initialized"}},
        };
        InstanceInfo ii3 = {
            .service_status = InstanceServiceStatus::kInService,
            .private_ip_address = "ip3",
            .labels = {{"shard-num", "1"}, {"initialized", "initialized"}},
        };
        InstanceInfo ii4 = {
            .service_status = InstanceServiceStatus::kPreService,
            .private_ip_address = "ip4",
            .labels = {{"shard-num", "1"}},
        };
        InstanceInfo ii5 = {
            .service_status = InstanceServiceStatus::kPreService,
            .private_ip_address = "ip5",
            .labels = {{"shard-num", "1"}},
        };
        std::vector<InstanceInfo> instances{ii1, ii2, ii3, ii4, ii5};
        return instances;
      });
  MockParameterFetcher parameter_fetcher;
  EXPECT_CALL(parameter_fetcher,
              GetParameter("project-id", testing::Eq(std::nullopt)))
      .WillOnce(testing::Return(project_id));
  auto mgr = ClusterMappingsManager::Create(
      environment, num_shards, *instance_client, parameter_fetcher);
  auto cluster_mappings = mgr->GetClusterMappings();
  EXPECT_EQ(cluster_mappings.size(), 4);
  absl::flat_hash_set<std::string> set0 = {"ip1", "ip2"};
  EXPECT_THAT(cluster_mappings[0], testing::UnorderedElementsAreArray(set0));
  absl::flat_hash_set<std::string> set1 = {"ip3"};
  EXPECT_THAT(cluster_mappings[1], testing::UnorderedElementsAreArray(set1));
  absl::flat_hash_set<std::string> set2;
  EXPECT_THAT(cluster_mappings[2], testing::UnorderedElementsAreArray(set2));
  EXPECT_THAT(cluster_mappings[3], testing::UnorderedElementsAreArray(set2));
}

TEST_F(ClusterMappingsGcpTest, RetrieveMappingsWithRetrySuccessfully) {
  std::string environment = "testenv";
  std::string project_id = "some-project-id";
  int32_t num_shards = 2;
  auto instance_client = std::make_unique<MockInstanceClient>();
  EXPECT_CALL(*instance_client, DescribeInstanceGroupInstances(::testing::_))
      .WillOnce(testing::Return(absl::InternalError("Oops.")))
      .WillOnce([&](DescribeInstanceGroupInput& input) {
        auto gcp_describe_instance_group_input =
            std::get_if<GcpDescribeInstanceGroupInput>(&input);
        EXPECT_EQ(gcp_describe_instance_group_input->project_id, project_id);
        InstanceInfo ii1 = {
            .service_status = InstanceServiceStatus::kInService,
            .private_ip_address = "ip1",
            .labels = {{"shard-num", "0"}, {"initialized", "initialized"}},
        };
        std::vector<InstanceInfo> instances{ii1};
        return instances;
      });
  MockParameterFetcher parameter_fetcher;
  EXPECT_CALL(parameter_fetcher,
              GetParameter("project-id", testing::Eq(std::nullopt)))
      .WillOnce(testing::Return(project_id));
  auto mgr = ClusterMappingsManager::Create(
      environment, num_shards, *instance_client, parameter_fetcher);
  auto cluster_mappings = mgr->GetClusterMappings();
  EXPECT_EQ(cluster_mappings.size(), 2);
  absl::flat_hash_set<std::string> set0 = {"ip1"};
  EXPECT_THAT(cluster_mappings[0], testing::UnorderedElementsAreArray(set0));
  absl::flat_hash_set<std::string> set1 = {};
  EXPECT_THAT(cluster_mappings[1], testing::UnorderedElementsAreArray(set1));
}

TEST_F(ClusterMappingsGcpTest, UpdateMappings) {
  std::string environment = "testenv";
  std::string project_id = "some-project-id";
  int32_t num_shards = 2;
  privacy_sandbox::server_common::MockMetricsRecorder mock_metrics_recorder;
  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager;
  auto instance_client = std::make_unique<MockInstanceClient>();
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < num_shards; i++) {
    cluster_mappings.push_back({"some_ip"});
  }
  auto shard_manager_status =
      ShardManager::Create(num_shards, fake_key_fetcher_manager,
                           cluster_mappings, mock_metrics_recorder);
  ASSERT_TRUE(shard_manager_status.ok());
  auto shard_manager = std::move(*shard_manager_status);
  absl::Notification finished;
  EXPECT_CALL(*instance_client, DescribeInstanceGroupInstances(::testing::_))
      .WillOnce([&](DescribeInstanceGroupInput& input) {
        auto gcp_describe_instance_group_input =
            std::get_if<GcpDescribeInstanceGroupInput>(&input);
        EXPECT_EQ(gcp_describe_instance_group_input->project_id, project_id);
        InstanceInfo ii1 = {
            .service_status = InstanceServiceStatus::kInService,
            .private_ip_address = "ip10",
            .labels = {{"shard-num", "0"}, {"initialized", "initialized"}},
        };
        std::vector<InstanceInfo> instances{ii1};
        return instances;
      })
      .WillOnce([&](DescribeInstanceGroupInput& input) {
        auto gcp_describe_instance_group_input =
            std::get_if<GcpDescribeInstanceGroupInput>(&input);
        EXPECT_EQ(gcp_describe_instance_group_input->project_id, project_id);
        InstanceInfo ii1 = {
            .service_status = InstanceServiceStatus::kInService,
            .private_ip_address = "ip20",
            .labels = {{"shard-num", "0"}, {"initialized", "initialized"}},
        };
        std::vector<InstanceInfo> instances{ii1};
        finished.Notify();
        return instances;
      });
  MockParameterFetcher parameter_fetcher;
  EXPECT_CALL(parameter_fetcher,
              GetParameter("project-id", testing::Eq(std::nullopt)))
      .WillOnce(testing::Return(project_id));
  auto mgr = ClusterMappingsManager::Create(
      environment, num_shards, *instance_client, parameter_fetcher);
  mgr->Start(*shard_manager);
  finished.WaitForNotification();
  ASSERT_TRUE(mgr->Stop().ok());
  EXPECT_FALSE(mgr->IsRunning());
  auto latest_ip = shard_manager->Get(0)->GetIpAddress();
  EXPECT_EQ(latest_ip, absl::StrCat("ip20:", kRemoteLookupServerPort));
}

}  // namespace
}  // namespace kv_server
