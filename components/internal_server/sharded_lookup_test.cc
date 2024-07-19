// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/internal_server/sharded_lookup.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/internal_server/mocks.h"
#include "components/sharding/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "public/test_util/proto_matcher.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using testing::_;
using testing::Return;

class ShardedLookupTest : public ::testing::Test {
 protected:
  ShardedLookupTest() {
    InitMetricsContextMap();
    request_context_ = std::make_unique<RequestContext>();
    request_context_->UpdateLogContext(
        privacy_sandbox::server_common::LogContext(),
        privacy_sandbox::server_common::ConsentedDebugConfiguration());
    request_context_ = std::make_shared<RequestContext>();
  }
  RequestContext& GetRequestContext() { return *request_context_; }
  std::shared_ptr<RequestContext> request_context_;
  int32_t num_shards_ = 2;
  int32_t shard_num_ = 0;

  MockLookup mock_local_lookup_;
  KeySharder key_sharder_ = KeySharder(ShardingFunction{/*seed=*/""});
};

TEST_F(ShardedLookupTest, VerifyCorrectnessOfSerializedRequest) {
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  privacy_sandbox::server_common::ConsentedDebugConfiguration
      consented_debug_config;
  consented_debug_config.set_is_consented(true);
  consented_debug_config.set_token("test_token");
  privacy_sandbox::server_common::LogContext log_context;
  log_context.set_adtech_debug_id("debug_id");
  log_context.set_generation_id("generation_id");
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(),
      [&consented_debug_config, &log_context](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }
        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1", "key5"};
        EXPECT_CALL(*mock_remote_lookup_client_1, GetValues(_, _, 0))
            .WillOnce([=](const RequestContext& request_context,
                          const std::string_view serialized_message,
                          const int32_t padding_length) {
              InternalLookupRequest request;
              EXPECT_TRUE(request.ParseFromString(serialized_message));
              auto request_keys = std::vector<std::string_view>(
                  request.keys().begin(), request.keys().end());
              EXPECT_THAT(request.keys(),
                          testing::UnorderedElementsAreArray(key_list_remote));
              EXPECT_THAT(request.log_context(), EqualsProto(log_context));
              EXPECT_THAT(request.consented_debug_config(),
                          EqualsProto(consented_debug_config));
              InternalLookupResponse resp;
              SingleLookupResult result;
              auto status = result.mutable_status();
              status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
              (*resp.mutable_kv_pairs())["key1"] = result;
              return resp;
            });

        return mock_remote_lookup_client_1;
      });

  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto request_log_context =
      std::make_unique<RequestLogContext>(log_context, consented_debug_config);
  auto request_context = std::make_unique<RequestContext>();
  request_context->UpdateLogContext(log_context, consented_debug_config);
  EXPECT_TRUE(
      sharded_lookup->GetKeyValues(*request_context, {"key1", "key4", "key5"})
          .ok());
}

TEST_F(ShardedLookupTest, GetKeyValues_Success) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key4"
                                     value { value: "value4" }
                                   }
                              )pb",
                              &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetKeyValues(_, _))
      .WillOnce(Return(local_lookup_response));

  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [this](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }

        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1"};
        InternalLookupRequest request;
        request.mutable_keys()->Assign(key_list_remote.begin(),
                                       key_list_remote.end());
        *request.mutable_consented_debug_config() =
            GetRequestContext()
                .GetRequestLogContext()
                .GetConsentedDebugConfiguration();
        *request.mutable_log_context() =
            GetRequestContext().GetRequestLogContext().GetLogContext();
        const std::string serialized_request = request.SerializeAsString();
        EXPECT_CALL(*mock_remote_lookup_client_1,
                    GetValues(_, serialized_request, 0))
            .WillOnce([&]() {
              InternalLookupResponse resp;
              SingleLookupResult result;
              result.set_value("value1");
              (*resp.mutable_kv_pairs())["key1"] = result;
              return resp;
            });

        return mock_remote_lookup_client_1;
      });
  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response =
      sharded_lookup->GetKeyValues(GetRequestContext(), {"key1", "key4"});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   }
                                   kv_pairs {
                                     key: "key4"
                                     value { value: "value4" }
                                   }
                              )pb",
                              &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(ShardedLookupTest, GetKeyValues_KeyMissing_ReturnsStatus) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { value: "value4" }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetKeyValues(_, _))
      .WillOnce(Return(local_lookup_response));

  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }

  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [this](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }
        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1", "key5"};
        InternalLookupRequest request;
        request.mutable_keys()->Assign(key_list_remote.begin(),
                                       key_list_remote.end());
        *request.mutable_consented_debug_config() =
            GetRequestContext()
                .GetRequestLogContext()
                .GetConsentedDebugConfiguration();
        *request.mutable_log_context() =
            GetRequestContext().GetRequestLogContext().GetLogContext();
        const std::string serialized_request = request.SerializeAsString();

        EXPECT_CALL(*mock_remote_lookup_client_1, GetValues(_, _, 0))
            .WillOnce([=](const RequestContext& request_context,
                          const std::string_view serialized_message,
                          const int32_t padding_length) {
              InternalLookupRequest request;
              EXPECT_TRUE(request.ParseFromString(serialized_message));
              auto request_keys = std::vector<std::string_view>(
                  request.keys().begin(), request.keys().end());
              EXPECT_THAT(request.keys(),
                          testing::UnorderedElementsAreArray(key_list_remote));

              InternalLookupResponse resp;
              SingleLookupResult result;
              auto status = result.mutable_status();
              status->set_code(static_cast<int>(absl::StatusCode::kNotFound));

              (*resp.mutable_kv_pairs())["key1"] = result;
              return resp;
            });

        return mock_remote_lookup_client_1;
      });

  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->GetKeyValues(GetRequestContext(),
                                               {"key1", "key4", "key5"});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { status: { code: 5, message: "" } }
           }
           kv_pairs {
             key: "key4"
             value { value: "value4" }
           },
           kv_pairs {
             key: "key5"
             value { status: { code: 5, message: "" } }
           }
      )pb",
      &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(ShardedLookupTest, GetKeyValues_EmptyRequest_ReturnsEmptyResponse) {
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [](const std::string& ip) {
        return std::make_unique<MockRemoteLookupClient>();
      });
  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->GetKeyValues(GetRequestContext(), {});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(ShardedLookupTest, GetKeyValues_FailedDownstreamRequest) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { value: "value4" }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetKeyValues(_, _))
      .WillOnce(Return(local_lookup_response));

  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [this](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }
        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1"};
        InternalLookupRequest request;
        request.mutable_keys()->Assign(key_list_remote.begin(),
                                       key_list_remote.end());
        *request.mutable_consented_debug_config() =
            GetRequestContext()
                .GetRequestLogContext()
                .GetConsentedDebugConfiguration();
        *request.mutable_log_context() =
            GetRequestContext().GetRequestLogContext().GetLogContext();
        const std::string serialized_request = request.SerializeAsString();
        EXPECT_CALL(*mock_remote_lookup_client_1,
                    GetValues(_, serialized_request, 0))
            .WillOnce([]() { return absl::DeadlineExceededError("too long"); });

        return mock_remote_lookup_client_1;
      });

  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response =
      sharded_lookup->GetKeyValues(GetRequestContext(), {"key1", "key4"});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  TextFormat::ParseFromString(
      R"pb(
        kv_pairs {
          key: "key1"
          value { status { code: 13 message: "Data lookup failed" } }
        }
        kv_pairs {
          key: "key4"
          value { value: "value4" }
        })pb",
      &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(ShardedLookupTest, GetKeyValues_ReturnsKeysFromCachePadding) {
  auto num_shards = 4;
  absl::flat_hash_set<std::string_view> keys;
  // 0
  keys.insert("key4");
  keys.insert("verylongkey2");
  // 1
  keys.insert("key1");
  keys.insert("key2");
  keys.insert("key3");
  // 2
  keys.insert("randomkey5");
  // 3
  keys.insert("longkey1");
  keys.insert("randomkey3");

  int total_length = 26;

  std::vector<std::string> key_list = {"key4", "verylongkey2"};
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { value: "key4value" }
           }
           kv_pairs {
             key: "verylongkey2"
             value { value: "verylongkey2value" }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetKeyValues(_, _))
      .WillOnce([&key_list, &local_lookup_response](
                    const RequestContext& request_context,
                    absl::flat_hash_set<std::string_view> key_list_input) {
        EXPECT_THAT(key_list,
                    testing::UnorderedElementsAreArray(key_list_input));
        return local_lookup_response;
      });

  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < num_shards; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(),
      [total_length, this](const std::string& ip) {
        if (ip == "1") {
          auto mock_remote_lookup_client_1 =
              std::make_unique<MockRemoteLookupClient>();
          const std::vector<std::string_view> key_list_remote = {"key1", "key2",
                                                                 "key3"};
          InternalLookupRequest request;
          request.mutable_keys()->Assign(key_list_remote.begin(),
                                         key_list_remote.end());
          *request.mutable_consented_debug_config() =
              GetRequestContext()
                  .GetRequestLogContext()
                  .GetConsentedDebugConfiguration();
          *request.mutable_log_context() =
              GetRequestContext().GetRequestLogContext().GetLogContext();
          const std::string serialized_request = request.SerializeAsString();
          EXPECT_CALL(*mock_remote_lookup_client_1, GetValues(_, _, _))
              .WillOnce([total_length, key_list_remote](
                            const RequestContext& request_context,
                            const std::string_view serialized_message,
                            const int32_t padding_length) {
                EXPECT_EQ(total_length,
                          (serialized_message.size() + padding_length));
                InternalLookupRequest request;
                EXPECT_TRUE(request.ParseFromString(serialized_message));
                auto request_keys = std::vector<std::string_view>(
                    request.keys().begin(), request.keys().end());
                EXPECT_THAT(request.keys(), testing::UnorderedElementsAreArray(
                                                key_list_remote));
                InternalLookupResponse resp;
                SingleLookupResult result;
                result.set_value("value1");
                (*resp.mutable_kv_pairs())["key1"] = result;
                SingleLookupResult result2;
                result2.set_value("value2");
                (*resp.mutable_kv_pairs())["key2"] = result2;
                SingleLookupResult result3;
                result3.set_value("value3");
                (*resp.mutable_kv_pairs())["key3"] = result3;
                return resp;
              });

          return mock_remote_lookup_client_1;
        }
        if (ip == "2") {
          auto mock_remote_lookup_client_1 =
              std::make_unique<MockRemoteLookupClient>();
          const std::vector<std::string_view> key_list_remote = {"randomkey5"};
          InternalLookupRequest request;
          request.mutable_keys()->Assign(key_list_remote.begin(),
                                         key_list_remote.end());
          *request.mutable_consented_debug_config() =
              GetRequestContext()
                  .GetRequestLogContext()
                  .GetConsentedDebugConfiguration();
          *request.mutable_log_context() =
              GetRequestContext().GetRequestLogContext().GetLogContext();
          const std::string serialized_request = request.SerializeAsString();
          EXPECT_CALL(*mock_remote_lookup_client_1,
                      GetValues(_, serialized_request, _))
              .WillOnce([&](const RequestContext& request_context,
                            const std::string_view serialized_message,
                            const int32_t padding_length) {
                InternalLookupResponse resp;
                return resp;
              });

          return mock_remote_lookup_client_1;
        }
        if (ip == "3") {
          auto mock_remote_lookup_client_1 =
              std::make_unique<MockRemoteLookupClient>();
          const std::vector<std::string_view> key_list_remote = {"longkey1",
                                                                 "randomkey3"};
          InternalLookupRequest request;
          request.mutable_keys()->Assign(key_list_remote.begin(),
                                         key_list_remote.end());
          *request.mutable_consented_debug_config() =
              GetRequestContext()
                  .GetRequestLogContext()
                  .GetConsentedDebugConfiguration();
          *request.mutable_log_context() =
              GetRequestContext().GetRequestLogContext().GetLogContext();
          const std::string serialized_request = request.SerializeAsString();
          EXPECT_CALL(*mock_remote_lookup_client_1, GetValues(_, _, _))
              .WillOnce([=](const RequestContext& request_context,
                            const std::string_view serialized_message,
                            const int32_t padding_length) {
                InternalLookupRequest request;
                EXPECT_TRUE(request.ParseFromString(serialized_message));
                auto request_keys = std::vector<std::string_view>(
                    request.keys().begin(), request.keys().end());
                EXPECT_THAT(request.keys(), testing::UnorderedElementsAreArray(
                                                key_list_remote));

                EXPECT_EQ(total_length,
                          (serialized_message.size() + padding_length));
                InternalLookupResponse resp;
                SingleLookupResult result;
                result.set_value("longkey1value");
                (*resp.mutable_kv_pairs())["longkey1"] = result;
                SingleLookupResult result2;
                result2.set_value("randomkey3value");
                (*resp.mutable_kv_pairs())["randomkey3"] = result2;
                return resp;
              });

          return mock_remote_lookup_client_1;
        }
        // ip == "0"
        return std::make_unique<MockRemoteLookupClient>();
      });

  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->GetKeyValues(GetRequestContext(), keys);
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  TextFormat::ParseFromString(
      R"pb(
        kv_pairs {
          key: "key1"
          value { value: "value1" }
        }
        kv_pairs {
          key: "key2"
          value { value: "value2" }
        }
        kv_pairs {
          key: "key3"
          value { value: "value3" }
        }
        kv_pairs {
          key: "key4"
          value { value: "key4value" }
        }
        kv_pairs {
          key: "longkey1"
          value { value: "longkey1value" }
        }
        kv_pairs {
          key: "randomkey3"
          value { value: "randomkey3value" }
        }
        kv_pairs {
          key: "randomkey5"
          value { status { code: 5 message: "" } }
        }
        kv_pairs { key: "verylongkey2"
                   value { value: "verylongkey2value" }
      )pb",
      &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(ShardedLookupTest, GetKeyValueSets_KeysFound_Success) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { keyset_values { values: "value4" } }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetKeyValueSet(_, _))
      .WillOnce(Return(local_lookup_response));

  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }

  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [this](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }

        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1"};
        InternalLookupRequest request;
        request.mutable_keys()->Assign(key_list_remote.begin(),
                                       key_list_remote.end());
        *request.mutable_consented_debug_config() =
            GetRequestContext()
                .GetRequestLogContext()
                .GetConsentedDebugConfiguration();
        *request.mutable_log_context() =
            GetRequestContext().GetRequestLogContext().GetLogContext();
        request.set_lookup_sets(true);
        const std::string serialized_request = request.SerializeAsString();
        EXPECT_CALL(*mock_remote_lookup_client_1, GetValues(_, _, 0))
            .WillOnce([&]() {
              InternalLookupResponse resp;
              TextFormat::ParseFromString(
                  R"pb(kv_pairs {
                         key: "key1"
                         value { keyset_values { values: "value1" } }
                       }
                  )pb",
                  &resp);
              return resp;
            });

        return mock_remote_lookup_client_1;
      });

  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response =
      sharded_lookup->GetKeyValueSet(GetRequestContext(), {"key1", "key4"});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { keyset_values { values: "value1" } }
           }
           kv_pairs {
             key: "key4"
             value { keyset_values { values: "value4" } }
           }
      )pb",
      &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(ShardedLookupTest, GetKeyValueSets_KeysMissing_ReturnsStatus) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { keyset_values { values: "value4" } }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetKeyValueSet(_, _))
      .WillOnce(Return(local_lookup_response));

  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }

  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [this](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }

        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1", "key5"};
        InternalLookupRequest request;
        request.mutable_keys()->Assign(key_list_remote.begin(),
                                       key_list_remote.end());
        request.set_lookup_sets(true);
        *request.mutable_consented_debug_config() =
            GetRequestContext()
                .GetRequestLogContext()
                .GetConsentedDebugConfiguration();
        *request.mutable_log_context() =
            GetRequestContext().GetRequestLogContext().GetLogContext();
        const std::string serialized_request = request.SerializeAsString();
        EXPECT_CALL(*mock_remote_lookup_client_1, GetValues(_, _, 0))
            .WillOnce([=](const RequestContext& request_context,
                          const std::string_view serialized_message,
                          const int32_t padding_length) {
              InternalLookupRequest request;
              EXPECT_TRUE(request.ParseFromString(serialized_message));
              auto request_keys = std::vector<std::string_view>(
                  request.keys().begin(), request.keys().end());
              EXPECT_THAT(request.keys(),
                          testing::UnorderedElementsAreArray(key_list_remote));

              InternalLookupResponse resp;
              SingleLookupResult result;
              auto status = result.mutable_status();
              status->set_code(static_cast<int>(absl::StatusCode::kNotFound));

              (*resp.mutable_kv_pairs())["key1"] = result;
              return resp;
            });

        return mock_remote_lookup_client_1;
      });

  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->GetKeyValueSet(GetRequestContext(),
                                                 {"key1", "key4", "key5"});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { status: { code: 5, message: "" } }
           }
           kv_pairs {
             key: "key4"
             value { keyset_values { values: "value4" } }
           }
           kv_pairs {
             key: "key5"
             value { status: { code: 5, message: "" } }
           }
      )pb",
      &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(ShardedLookupTest, GetUInt32ValueSets_KeysFound_Success) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { uint32set_values { values: 1000 } }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetUInt32ValueSet(_, _))
      .WillOnce(Return(local_lookup_response));
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [this](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }
        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1"};
        InternalLookupRequest request;
        request.mutable_keys()->Assign(key_list_remote.begin(),
                                       key_list_remote.end());
        *request.mutable_consented_debug_config() =
            GetRequestContext()
                .GetRequestLogContext()
                .GetConsentedDebugConfiguration();
        *request.mutable_log_context() =
            GetRequestContext().GetRequestLogContext().GetLogContext();
        request.set_lookup_sets(true);
        const std::string serialized_request = request.SerializeAsString();
        EXPECT_CALL(*mock_remote_lookup_client_1, GetValues(_, _, 0))
            .WillOnce([&]() {
              InternalLookupResponse resp;
              TextFormat::ParseFromString(
                  R"pb(kv_pairs {
                         key: "key1"
                         value { uint32set_values { values: 2000 } }
                       }
                  )pb",
                  &resp);
              return resp;
            });
        return mock_remote_lookup_client_1;
      });
  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response =
      sharded_lookup->GetUInt32ValueSet(GetRequestContext(), {"key1", "key4"});
  ASSERT_TRUE(response.ok());
  InternalLookupResponse expected;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { uint32set_values { values: 2000 } }
           }
           kv_pairs {
             key: "key4"
             value { uint32set_values { values: 1000 } }
           }
      )pb",
      &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(ShardedLookupTest, GetUInt32ValueSets_KeysMissing_ReturnsStatus) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { uint32set_values { values: 1000 } }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetUInt32ValueSet(_, _))
      .WillOnce(Return(local_lookup_response));
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [this](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }
        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1", "key5"};
        InternalLookupRequest request;
        request.mutable_keys()->Assign(key_list_remote.begin(),
                                       key_list_remote.end());
        request.set_lookup_sets(true);
        *request.mutable_consented_debug_config() =
            GetRequestContext()
                .GetRequestLogContext()
                .GetConsentedDebugConfiguration();
        *request.mutable_log_context() =
            GetRequestContext().GetRequestLogContext().GetLogContext();
        const std::string serialized_request = request.SerializeAsString();
        EXPECT_CALL(*mock_remote_lookup_client_1, GetValues(_, _, 0))
            .WillOnce([=](const RequestContext& request_context,
                          const std::string_view serialized_message,
                          const int32_t padding_length) {
              InternalLookupRequest request;
              EXPECT_TRUE(request.ParseFromString(serialized_message));
              auto request_keys = std::vector<std::string_view>(
                  request.keys().begin(), request.keys().end());
              EXPECT_THAT(request.keys(),
                          testing::UnorderedElementsAreArray(key_list_remote));
              InternalLookupResponse resp;
              SingleLookupResult result;
              auto status = result.mutable_status();
              status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
              (*resp.mutable_kv_pairs())["key1"] = result;
              return resp;
            });
        return mock_remote_lookup_client_1;
      });
  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->GetUInt32ValueSet(GetRequestContext(),
                                                    {"key1", "key4", "key5"});
  ASSERT_TRUE(response.ok());
  InternalLookupResponse expected;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { status: { code: 5, message: "" } }
           }
           kv_pairs {
             key: "key4"
             value { uint32set_values { values: 1000 } }
           }
           kv_pairs {
             key: "key5"
             value { status: { code: 5, message: "" } }
           }
      )pb",
      &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(ShardedLookupTest, GetUInt32ValueSet_EmptyRequest_ReturnsEmptyResponse) {
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [](const std::string& ip) {
        return std::make_unique<MockRemoteLookupClient>();
      });
  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->GetUInt32ValueSet(GetRequestContext(), {});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(ShardedLookupTest, GetKeyValueSet_EmptyRequest_ReturnsEmptyResponse) {
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [](const std::string& ip) {
        return std::make_unique<MockRemoteLookupClient>();
      });
  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->GetKeyValueSet(GetRequestContext(), {});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(ShardedLookupTest, GetKeyValueSet_FailedDownstreamRequest) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { keyset_values { values: "value4" } }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetKeyValueSet(_, _))
      .WillOnce(Return(local_lookup_response));

  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [this](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }
        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1"};
        InternalLookupRequest request;
        request.mutable_keys()->Assign(key_list_remote.begin(),
                                       key_list_remote.end());
        request.set_lookup_sets(true);
        *request.mutable_consented_debug_config() =
            GetRequestContext()
                .GetRequestLogContext()
                .GetConsentedDebugConfiguration();
        *request.mutable_log_context() =
            GetRequestContext().GetRequestLogContext().GetLogContext();
        const std::string serialized_request = request.SerializeAsString();
        EXPECT_CALL(*mock_remote_lookup_client_1,
                    GetValues(_, serialized_request, 0))
            .WillOnce([]() { return absl::DeadlineExceededError("too long"); });

        return mock_remote_lookup_client_1;
      });

  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response =
      sharded_lookup->GetKeyValueSet(GetRequestContext(), {"key1", "key4"});
  EXPECT_FALSE(response.ok());
  EXPECT_EQ(response.status().code(), absl::StatusCode::kDeadlineExceeded);
}

TEST_F(ShardedLookupTest, RunQuery_Success) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { keyset_values { values: "value4" } }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetKeyValueSet(_, _))
      .WillOnce(Return(local_lookup_response));

  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [this](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }

        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1"};
        InternalLookupRequest request;
        request.mutable_keys()->Assign(key_list_remote.begin(),
                                       key_list_remote.end());
        request.set_lookup_sets(true);
        *request.mutable_consented_debug_config() =
            GetRequestContext()
                .GetRequestLogContext()
                .GetConsentedDebugConfiguration();
        *request.mutable_log_context() =
            GetRequestContext().GetRequestLogContext().GetLogContext();
        const std::string serialized_request = request.SerializeAsString();
        EXPECT_CALL(*mock_remote_lookup_client_1,
                    GetValues(_, serialized_request, 0))
            .WillOnce([&]() {
              InternalLookupResponse resp;
              TextFormat::ParseFromString(
                  R"pb(kv_pairs {
                         key: "key1"
                         value { keyset_values { values: "value1" } }
                       }
                  )pb",
                  &resp);
              return resp;
            });

        return mock_remote_lookup_client_1;
      });

  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->RunQuery(GetRequestContext(), "key1|key4");
  EXPECT_TRUE(response.ok());

  EXPECT_THAT(response.value().elements(),
              testing::UnorderedElementsAreArray({"value1", "value4"}));
}

TEST_F(ShardedLookupTest, RunQuery_MissingKeySet_IgnoresMissingSet_Success) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { status { code: 5 } }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetKeyValueSet(_, _))
      .WillOnce(Return(local_lookup_response));

  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [this](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }

        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1"};
        InternalLookupRequest request;
        request.mutable_keys()->Assign(key_list_remote.begin(),
                                       key_list_remote.end());
        request.set_lookup_sets(true);
        *request.mutable_consented_debug_config() =
            GetRequestContext()
                .GetRequestLogContext()
                .GetConsentedDebugConfiguration();
        *request.mutable_log_context() =
            GetRequestContext().GetRequestLogContext().GetLogContext();
        const std::string serialized_request = request.SerializeAsString();
        EXPECT_CALL(*mock_remote_lookup_client_1,
                    GetValues(_, serialized_request, 0))
            .WillOnce([&]() {
              InternalLookupResponse resp;
              TextFormat::ParseFromString(
                  R"pb(kv_pairs {
                         key: "key1"
                         value { keyset_values { values: "value1" } }
                       }
                  )pb",
                  &resp);
              return resp;
            });

        return mock_remote_lookup_client_1;
      });

  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->RunQuery(GetRequestContext(), "key1|key4");
  EXPECT_TRUE(response.ok());

  EXPECT_THAT(response.value().elements(),
              testing::UnorderedElementsAreArray({"value1"}));
}

TEST_F(ShardedLookupTest, RunQuery_ShardedLookupFails_Error) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { keyset_values { values: "value4" } }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetKeyValueSet(_, _))
      .WillOnce(Return(local_lookup_response));

  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager =
      ShardManager::Create(num_shards_, std::move(cluster_mappings),
                           std::make_unique<MockRandomGenerator>(),
                           [](const std::string& ip) { return nullptr; });

  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->RunQuery(GetRequestContext(), "key1|key4");
  EXPECT_FALSE(response.ok());

  EXPECT_THAT(response.status().code(), absl::StatusCode::kInternal);
}

TEST_F(ShardedLookupTest, RunQuery_ParseError_ReturnStatus) {
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [](const std::string& ip) {
        return std::make_unique<MockRemoteLookupClient>();
      });

  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->RunQuery(GetRequestContext(), "key1|");
  EXPECT_FALSE(response.ok());

  EXPECT_EQ(response.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(ShardedLookupTest, RunQuery_EmptyRequest_EmptyResponse) {
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [](const std::string& ip) {
        return std::make_unique<MockRemoteLookupClient>();
      });

  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->RunQuery(GetRequestContext(), "");
  EXPECT_TRUE(response.ok());
  EXPECT_TRUE(response.value().elements().empty());
}

TEST_F(ShardedLookupTest, RunSetQueryUInt32_Success) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { uint32set_values { values: 1000 } }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetUInt32ValueSet(_, _))
      .WillOnce(Return(local_lookup_response));
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [this](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }
        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1"};
        InternalLookupRequest request;
        request.mutable_keys()->Assign(key_list_remote.begin(),
                                       key_list_remote.end());
        request.set_lookup_sets(true);
        *request.mutable_consented_debug_config() =
            GetRequestContext()
                .GetRequestLogContext()
                .GetConsentedDebugConfiguration();
        *request.mutable_log_context() =
            GetRequestContext().GetRequestLogContext().GetLogContext();
        const std::string serialized_request = request.SerializeAsString();
        EXPECT_CALL(*mock_remote_lookup_client_1,
                    GetValues(_, serialized_request, 0))
            .WillOnce([&]() {
              InternalLookupResponse resp;
              TextFormat::ParseFromString(
                  R"pb(kv_pairs {
                         key: "key1"
                         value { uint32set_values { values: 2000 } }
                       }
                  )pb",
                  &resp);
              return resp;
            });
        return mock_remote_lookup_client_1;
      });
  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response =
      sharded_lookup->RunSetQueryUInt32(GetRequestContext(), "key1|key4");
  EXPECT_TRUE(response.ok());
  EXPECT_THAT(response.value().elements(),
              testing::UnorderedElementsAreArray({1000, 2000}));
}

TEST_F(ShardedLookupTest, RunSetQueryUInt32_ShardedLookupFails_Error) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { uint32set_values { values: 1000 } }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetUInt32ValueSet(_, _))
      .WillOnce(Return(local_lookup_response));
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager =
      ShardManager::Create(num_shards_, std::move(cluster_mappings),
                           std::make_unique<MockRandomGenerator>(),
                           [](const std::string& ip) { return nullptr; });
  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response =
      sharded_lookup->RunSetQueryUInt32(GetRequestContext(), "key1|key4");
  EXPECT_FALSE(response.ok());
  EXPECT_THAT(response.status().code(), absl::StatusCode::kInternal);
}

TEST_F(ShardedLookupTest, RunSetQueryUInt32_EmptyRequest_EmptyResponse) {
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [](const std::string& ip) {
        return std::make_unique<MockRemoteLookupClient>();
      });

  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->RunSetQueryUInt32(GetRequestContext(), "");
  EXPECT_TRUE(response.ok());
  EXPECT_TRUE(response.value().elements().empty());
}

TEST_F(ShardedLookupTest, RunSetQueryUInt64_Success) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { uint64set_values { values: 18446744073709551 } }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetUInt64ValueSet(_, _))
      .WillOnce(Return(local_lookup_response));
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [this](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }
        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1"};
        InternalLookupRequest request;
        request.mutable_keys()->Assign(key_list_remote.begin(),
                                       key_list_remote.end());
        request.set_lookup_sets(true);
        *request.mutable_consented_debug_config() =
            GetRequestContext()
                .GetRequestLogContext()
                .GetConsentedDebugConfiguration();
        *request.mutable_log_context() =
            GetRequestContext().GetRequestLogContext().GetLogContext();
        const std::string serialized_request = request.SerializeAsString();
        EXPECT_CALL(*mock_remote_lookup_client_1,
                    GetValues(_, serialized_request, 0))
            .WillOnce([&]() {
              InternalLookupResponse resp;
              TextFormat::ParseFromString(
                  R"pb(kv_pairs {
                         key: "key1"
                         value {
                           uint64set_values { values: 18446744073709552 }
                         }
                       }
                  )pb",
                  &resp);
              return resp;
            });
        return mock_remote_lookup_client_1;
      });
  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response =
      sharded_lookup->RunSetQueryUInt64(GetRequestContext(), "key1|key4");
  EXPECT_TRUE(response.ok());
  EXPECT_THAT(response.value().elements(),
              testing::UnorderedElementsAreArray(
                  {18446744073709551, 18446744073709552}));
}

TEST_F(ShardedLookupTest, RunSetQueryUInt64_ShardedLookupFails_Error) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { uint64set_values { values: 18446744073709551 } }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetUInt64ValueSet(_, _))
      .WillOnce(Return(local_lookup_response));
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager =
      ShardManager::Create(num_shards_, std::move(cluster_mappings),
                           std::make_unique<MockRandomGenerator>(),
                           [](const std::string& ip) { return nullptr; });
  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response =
      sharded_lookup->RunSetQueryUInt64(GetRequestContext(), "key1|key4");
  EXPECT_FALSE(response.ok());
  EXPECT_THAT(response.status().code(), absl::StatusCode::kInternal);
}

TEST_F(ShardedLookupTest, RunSetQueryUInt64_EmptyRequest_EmptyResponse) {
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [](const std::string& ip) {
        return std::make_unique<MockRemoteLookupClient>();
      });
  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->RunSetQueryUInt64(GetRequestContext(), "");
  EXPECT_TRUE(response.ok());
  EXPECT_TRUE(response.value().elements().empty());
}

TEST_F(ShardedLookupTest, GetUInt64ValueSets_KeysMissing_ReturnsStatus) {
  InternalLookupResponse local_lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key4"
             value { uint64set_values { values: 18446744073709551 } }
           }
      )pb",
      &local_lookup_response);
  EXPECT_CALL(mock_local_lookup_, GetUInt64ValueSet(_, _))
      .WillOnce(Return(local_lookup_response));
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [this](const std::string& ip) {
        if (ip != "1") {
          return std::make_unique<MockRemoteLookupClient>();
        }
        auto mock_remote_lookup_client_1 =
            std::make_unique<MockRemoteLookupClient>();
        const std::vector<std::string_view> key_list_remote = {"key1", "key5"};
        InternalLookupRequest request;
        request.mutable_keys()->Assign(key_list_remote.begin(),
                                       key_list_remote.end());
        request.set_lookup_sets(true);
        *request.mutable_consented_debug_config() =
            GetRequestContext()
                .GetRequestLogContext()
                .GetConsentedDebugConfiguration();
        *request.mutable_log_context() =
            GetRequestContext().GetRequestLogContext().GetLogContext();
        const std::string serialized_request = request.SerializeAsString();
        EXPECT_CALL(*mock_remote_lookup_client_1, GetValues(_, _, 0))
            .WillOnce([=](const RequestContext& request_context,
                          const std::string_view serialized_message,
                          const int32_t padding_length) {
              InternalLookupRequest request;
              EXPECT_TRUE(request.ParseFromString(serialized_message));
              auto request_keys = std::vector<std::string_view>(
                  request.keys().begin(), request.keys().end());
              EXPECT_THAT(request.keys(),
                          testing::UnorderedElementsAreArray(key_list_remote));
              InternalLookupResponse resp;
              SingleLookupResult result;
              auto status = result.mutable_status();
              status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
              (*resp.mutable_kv_pairs())["key1"] = result;
              return resp;
            });
        return mock_remote_lookup_client_1;
      });
  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->GetUInt64ValueSet(GetRequestContext(),
                                                    {"key1", "key4", "key5"});
  ASSERT_TRUE(response.ok());
  InternalLookupResponse expected;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { status: { code: 5, message: "" } }
           }
           kv_pairs {
             key: "key4"
             value { uint64set_values { values: 18446744073709551 } }
           }
           kv_pairs {
             key: "key5"
             value { status: { code: 5, message: "" } }
           }
      )pb",
      &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(ShardedLookupTest, GetUInt64ValueSet_EmptyRequest_ReturnsEmptyResponse) {
  std::vector<absl::flat_hash_set<std::string>> cluster_mappings;
  for (int i = 0; i < 2; i++) {
    cluster_mappings.push_back({std::to_string(i)});
  }
  auto shard_manager = ShardManager::Create(
      num_shards_, std::move(cluster_mappings),
      std::make_unique<MockRandomGenerator>(), [](const std::string& ip) {
        return std::make_unique<MockRemoteLookupClient>();
      });
  auto sharded_lookup =
      CreateShardedLookup(mock_local_lookup_, num_shards_, shard_num_,
                          *(*shard_manager), key_sharder_);
  auto response = sharded_lookup->GetUInt64ValueSet(GetRequestContext(), {});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

}  // namespace

}  // namespace kv_server
