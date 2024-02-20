// Copyright 2022 Google LLC
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

#include <string>
#include <utility>
#include <vector>

#include "components/internal_server/constants.h"
#include "components/sharding/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "src/cpp/encryption/key_fetcher/src/fake_key_fetcher_manager.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::FakeKeyFetcherManager;

class ShardManagerTest : public ::testing::Test {
 protected:
  FakeKeyFetcherManager fake_key_fetcher_manager_;
};

TEST_F(ShardManagerTest, CreationNotInitialized) {
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  auto shard_manager = ShardManager::Create(4, fake_key_fetcher_manager_,
                                            std::move(cluster_mappings));
  ASSERT_FALSE(shard_manager.ok());
}

TEST_F(ShardManagerTest, CreationInitialized) {
  int32_t num_shards = 4;
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < num_shards; i++) {
    cluster_mappings.push_back({"some_ip"});
  }
  auto shard_manager = ShardManager::Create(
      num_shards, fake_key_fetcher_manager_, std::move(cluster_mappings));
  ASSERT_TRUE(shard_manager.ok());
}

TEST_F(ShardManagerTest, CreationNotInitializedMissingClusters) {
  int32_t num_shards = 4;
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({"some_ip"});
  }
  auto shard_manager = ShardManager::Create(
      num_shards, fake_key_fetcher_manager_, std::move(cluster_mappings));
  ASSERT_FALSE(shard_manager.ok());
}

TEST_F(ShardManagerTest, CreationNotInitializedMissingReplicas) {
  int32_t num_shards = 4;
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 3; i++) {
    cluster_mappings.push_back({"some_ip"});
  }
  cluster_mappings.push_back({});
  auto shard_manager = ShardManager::Create(
      num_shards, fake_key_fetcher_manager_, std::move(cluster_mappings));
  ASSERT_FALSE(shard_manager.ok());
}

TEST_F(ShardManagerTest, InsertRetrieveSuccess) {
  int32_t num_shards = 4;
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < num_shards; i++) {
    cluster_mappings.push_back({"some_ip"});
  }
  auto shard_manager = ShardManager::Create(
      num_shards, fake_key_fetcher_manager_, std::move(cluster_mappings));
  ASSERT_TRUE(shard_manager.ok());
  EXPECT_EQ(absl::StrCat("some_ip:", kRemoteLookupServerPort),
            (*shard_manager)->Get(0)->GetIpAddress());
}

TEST_F(ShardManagerTest, InsertMissingReplicasRetrieveSuccess) {
  int32_t num_shards = 4;
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < num_shards; i++) {
    cluster_mappings.push_back({"some_ip"});
  }
  auto shard_manager = ShardManager::Create(
      num_shards, fake_key_fetcher_manager_, std::move(cluster_mappings));
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings_2;
  for (int i = 0; i < 3; i++) {
    cluster_mappings_2.push_back({"some_ip"});
  }
  cluster_mappings_2.push_back({});
  (*shard_manager)->InsertBatch(std::move(cluster_mappings_2));
  EXPECT_EQ(absl::StrCat("some_ip:", kRemoteLookupServerPort),
            (*shard_manager)->Get(0)->GetIpAddress());
}

TEST_F(ShardManagerTest, InsertRetrieveTwoVersions) {
  auto random_generator = std::make_unique<MockRandomGenerator>();
  EXPECT_CALL(*random_generator, Get(testing::_))
      .WillOnce([]() { return 0; })
      .WillOnce([]() { return 1; });
  std::string instance_id_1 = "some_ip_1";
  std::string instance_id_2 = "some_ip_2";
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  cluster_mappings.push_back({instance_id_2, instance_id_1});
  for (int i = 0; i < 3; i++) {
    cluster_mappings.push_back({"some_ip_3"});
  }
  auto& fake_key_fetcher_manager = fake_key_fetcher_manager_;
  auto client_factory = [&fake_key_fetcher_manager](const std::string& ip) {
    return RemoteLookupClient::Create(ip, fake_key_fetcher_manager);
  };
  auto shard_manager =
      ShardManager::Create(4, std::move(cluster_mappings),
                           std::move(random_generator), client_factory);
  std::set<std::string> etalon = {
      absl::StrCat(instance_id_1, ":", kRemoteLookupServerPort),
      absl::StrCat(instance_id_2, ":", kRemoteLookupServerPort)};
  std::set<std::string> result;
  result.insert(std::string((*shard_manager)->Get(0)->GetIpAddress()));
  result.insert(std::string((*shard_manager)->Get(0)->GetIpAddress()));
  EXPECT_EQ(etalon, result);
}

}  // namespace
}  // namespace kv_server
