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

class ClusterMappingsAwsTest : public ::testing::Test {
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

TEST_F(ClusterMappingsAwsTest, RetrieveMappingsSuccessfully) {
  std::string environment = "testenv";
  int32_t num_shards = 4;
  privacy_sandbox::server_common::MockMetricsRecorder mock_metrics_recorder;
  auto instance_client = std::make_unique<MockInstanceClient>();
  EXPECT_CALL(*instance_client, DescribeInstanceGroupInstances(::testing::_))
      .WillOnce([&](DescribeInstanceGroupInput& input) {
        auto aws_describe_instance_group_input =
            std::get_if<AwsDescribeInstanceGroupInput>(&input);
        absl::flat_hash_set<std::string> instance_group_names_expected = {
            "kv-server-testenv-0-instance-asg",
            "kv-server-testenv-1-instance-asg",
            "kv-server-testenv-2-instance-asg",
            "kv-server-testenv-3-instance-asg"};
        EXPECT_THAT(
            aws_describe_instance_group_input->instance_group_names,
            testing::UnorderedElementsAreArray(instance_group_names_expected));
        InstanceInfo ii1 = {
            .id = "id1",
            .instance_group = "kv-server-testenv-0-instance-asg",
            .service_status = InstanceServiceStatus::kInService,
        };
        InstanceInfo ii2 = {
            .id = "id2",
            .instance_group = "kv-server-testenv-0-instance-asg",
            .service_status = InstanceServiceStatus::kInService,
        };
        InstanceInfo ii3 = {
            .id = "id3",
            .instance_group = "kv-server-testenv-1-instance-asg",
            .service_status = InstanceServiceStatus::kInService,
        };
        InstanceInfo ii4 = {
            .id = "id4",
            .instance_group = "kv-server-testenv-2-instance-asg",
            .service_status = InstanceServiceStatus::kPreService,
        };
        InstanceInfo ii5 = {
            .id = "id5",
            .instance_group = "garbage",
            .service_status = InstanceServiceStatus::kPreService,
        };
        std::vector<InstanceInfo> instances{ii1, ii2, ii3, ii4, ii5};
        return instances;
      });

  EXPECT_CALL(*instance_client, DescribeInstances(::testing::_))
      .WillOnce(
          [&](const absl::flat_hash_set<std::string>& instance_group_names) {
            absl::flat_hash_set<std::string> instance_group_names_expected = {
                "id1", "id2", "id3"};

            EXPECT_THAT(instance_group_names,
                        testing::UnorderedElementsAreArray(
                            instance_group_names_expected));

            InstanceInfo ii1 = {.id = "id1", .private_ip_address = "ip1"};
            InstanceInfo ii2 = {.id = "id2", .private_ip_address = "ip2"};
            InstanceInfo ii3 = {.id = "id3", .private_ip_address = "ip3"};

            std::vector<InstanceInfo> instances{ii1, ii2, ii3};
            return instances;
          });

  MockParameterFetcher parameter_fetcher;
  auto mgr = ClusterMappingsManager::Create(
      environment, num_shards, mock_metrics_recorder, *instance_client,
      parameter_fetcher);
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

TEST_F(ClusterMappingsAwsTest, RetrieveMappingsWithRetrySuccessfully) {
  std::string environment = "testenv";
  int32_t num_shards = 2;
  privacy_sandbox::server_common::MockMetricsRecorder mock_metrics_recorder;
  auto instance_client = std::make_unique<MockInstanceClient>();
  EXPECT_CALL(*instance_client, DescribeInstanceGroupInstances(::testing::_))
      .WillOnce(testing::Return(absl::InternalError("Oops.")))
      .WillOnce([&](DescribeInstanceGroupInput& input) {
        auto aws_describe_instance_group_input =
            std::get_if<AwsDescribeInstanceGroupInput>(&input);
        absl::flat_hash_set<std::string> instance_group_names_expected = {
            "kv-server-testenv-0-instance-asg",
            "kv-server-testenv-1-instance-asg",
        };
        EXPECT_THAT(
            aws_describe_instance_group_input->instance_group_names,
            testing::UnorderedElementsAreArray(instance_group_names_expected));
        InstanceInfo ii1 = {
            .id = "id1",
            .instance_group = "kv-server-testenv-0-instance-asg",
            .service_status = InstanceServiceStatus::kInService};

        std::vector<InstanceInfo> instances{ii1};
        return instances;
      });

  EXPECT_CALL(*instance_client, DescribeInstances(::testing::_))
      .WillOnce(
          [&](const absl::flat_hash_set<std::string>& instance_group_names) {
            absl::flat_hash_set<std::string> instance_group_names_expected = {
                "id1"};

            EXPECT_THAT(instance_group_names,
                        testing::UnorderedElementsAreArray(
                            instance_group_names_expected));

            InstanceInfo ii1 = {.id = "id1", .private_ip_address = "ip1"};
            std::vector<InstanceInfo> instances{ii1};
            return instances;
          });
  MockParameterFetcher parameter_fetcher;
  auto mgr = ClusterMappingsManager::Create(
      environment, num_shards, mock_metrics_recorder, *instance_client,
      parameter_fetcher);
  auto cluster_mappings = mgr->GetClusterMappings();
  EXPECT_EQ(cluster_mappings.size(), 2);
  absl::flat_hash_set<std::string> set0 = {"ip1"};
  EXPECT_THAT(cluster_mappings[0], testing::UnorderedElementsAreArray(set0));
  absl::flat_hash_set<std::string> set1 = {};
  EXPECT_THAT(cluster_mappings[1], testing::UnorderedElementsAreArray(set1));
}

TEST_F(ClusterMappingsAwsTest, UpdateMappings) {
  std::string environment = "testenv";
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
        auto aws_describe_instance_group_input =
            std::get_if<AwsDescribeInstanceGroupInput>(&input);
        absl::flat_hash_set<std::string> instance_group_names_expected = {
            "kv-server-testenv-0-instance-asg",
            "kv-server-testenv-1-instance-asg",
        };
        EXPECT_THAT(
            aws_describe_instance_group_input->instance_group_names,
            testing::UnorderedElementsAreArray(instance_group_names_expected));
        InstanceInfo ii1 = {
            .id = "id10",
            .instance_group = "kv-server-testenv-0-instance-asg",
            .service_status = InstanceServiceStatus::kInService,
            .private_ip_address = "ip10"};

        std::vector<InstanceInfo> instances{ii1};
        return instances;
      })
      .WillOnce([&](DescribeInstanceGroupInput& input) {
        auto aws_describe_instance_group_input =
            std::get_if<AwsDescribeInstanceGroupInput>(&input);
        absl::flat_hash_set<std::string> instance_group_names_expected = {
            "kv-server-testenv-0-instance-asg",
            "kv-server-testenv-1-instance-asg",
        };
        EXPECT_THAT(
            aws_describe_instance_group_input->instance_group_names,
            testing::UnorderedElementsAreArray(instance_group_names_expected));
        InstanceInfo ii1 = {
            .id = "id20",
            .instance_group = "kv-server-testenv-0-instance-asg",
            .service_status = InstanceServiceStatus::kInService,
            .private_ip_address = "ip20"};

        std::vector<InstanceInfo> instances{ii1};

        finished.Notify();
        return instances;
      });

  EXPECT_CALL(*instance_client, DescribeInstances(::testing::_))
      .WillOnce(
          [&](const absl::flat_hash_set<std::string>& instance_group_names) {
            absl::flat_hash_set<std::string> instance_group_names_expected = {
                "id10"};

            EXPECT_THAT(instance_group_names,
                        testing::UnorderedElementsAreArray(
                            instance_group_names_expected));
            InstanceInfo ii1 = {.id = "id10", .private_ip_address = "ip10"};
            std::vector<InstanceInfo> instances{ii1};
            return instances;
          })
      .WillOnce(
          [&](const absl::flat_hash_set<std::string>& instance_group_names) {
            absl::flat_hash_set<std::string> instance_group_names_expected = {
                "id20"};

            EXPECT_THAT(instance_group_names,
                        testing::UnorderedElementsAreArray(
                            instance_group_names_expected));

            InstanceInfo ii1 = {.id = "id20", .private_ip_address = "ip20"};
            std::vector<InstanceInfo> instances{ii1};
            return instances;
          });
  MockParameterFetcher parameter_fetcher;
  auto mgr = ClusterMappingsManager::Create(
      environment, num_shards, mock_metrics_recorder, *instance_client,
      parameter_fetcher);
  mgr->Start(*shard_manager);
  finished.WaitForNotification();
  ASSERT_TRUE(mgr->Stop().ok());
  EXPECT_FALSE(mgr->IsRunning());
  auto latest_ip = shard_manager->Get(0)->GetIpAddress();
  EXPECT_EQ(latest_ip, absl::StrCat("ip20:", kRemoteLookupServerPort));
}

}  // namespace
}  // namespace kv_server
