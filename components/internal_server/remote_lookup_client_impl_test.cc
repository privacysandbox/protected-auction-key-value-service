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

#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/mocks.h"
#include "components/internal_server/lookup_server_impl.h"
#include "components/internal_server/remote_lookup_client.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "public/test_util/proto_matcher.h"
#include "src/cpp/encryption/key_fetcher/src/fake_key_fetcher_manager.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::MockMetricsRecorder;
using testing::_;
using testing::Return;

class RemoteLookupClientImplTest : public ::testing::Test {
 protected:
  RemoteLookupClientImplTest() {
    lookup_service_ = std::make_unique<LookupServiceImpl>(
        mock_cache_, fake_key_fetcher_manager_, mock_metrics_recorder_);
    grpc::ServerBuilder builder;
    builder.RegisterService(lookup_service_.get());
    server_ = (builder.BuildAndStart());
    remote_lookup_client_ = RemoteLookupClient::Create(
        InternalLookupService::NewStub(
            server_->InProcessChannel(grpc::ChannelArguments())),
        fake_key_fetcher_manager_, mock_metrics_recorder_);
  }

  ~RemoteLookupClientImplTest() {
    server_->Shutdown();
    server_->Wait();
  }
  MockCache mock_cache_;
  MockMetricsRecorder mock_metrics_recorder_;
  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager_;
  std::unique_ptr<LookupServiceImpl> lookup_service_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<RemoteLookupClient> remote_lookup_client_;
};

TEST_F(RemoteLookupClientImplTest, EncryptedPaddedSuccessfulCall) {
  std::vector<std::string> keys = {"key1", "key2"};
  InternalLookupRequest request;
  request.mutable_keys()->Assign(keys.begin(), keys.end());
  request.set_lookup_sets(false);
  std::string serialized_message = request.SerializeAsString();
  int32_t padding_length = 10;
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(testing::_))
      .WillOnce(testing::Return(absl::flat_hash_map<std::string, std::string>{
          {"key1", "value1"}, {"key2", "value2"}}));
  auto response_status =
      remote_lookup_client_->GetValues(serialized_message, padding_length);
  EXPECT_TRUE(response_status.ok());
  InternalLookupResponse response = *response_status;
  InternalLookupResponse expected;
  google::protobuf::TextFormat::ParseFromString(R"pb(kv_pairs {
                                                       key: "key1"
                                                       value { value: "value1" }
                                                     }
                                                     kv_pairs {
                                                       key: "key2"
                                                       value { value: "value2" }
                                                     }
                                                )pb",
                                                &expected);
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST_F(RemoteLookupClientImplTest, EncryptedPaddedEmptySuccessfulCall) {
  std::vector<std::string> keys = {};
  InternalLookupRequest request;
  request.mutable_keys()->Assign(keys.begin(), keys.end());
  request.set_lookup_sets(false);
  std::string serialized_message = request.SerializeAsString();
  int32_t padding_length = 10;
  auto response_status =
      remote_lookup_client_->GetValues(serialized_message, padding_length);
  EXPECT_TRUE(response_status.ok());
  InternalLookupResponse response = *response_status;
  InternalLookupResponse expected;
  google::protobuf::TextFormat::ParseFromString(R"pb()pb", &expected);
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST_F(RemoteLookupClientImplTest, EncryptedPaddedSuccessfulKeysettLookup) {
  std::vector<std::string> keys = {"key1"};
  InternalLookupRequest request;
  request.mutable_keys()->Assign(keys.begin(), keys.end());
  request.set_lookup_sets(true);
  std::string serialized_message = request.SerializeAsString();
  int32_t padding_length = 10;

  auto mock_get_key_value_set_result =
      std::make_unique<MockGetKeyValueSetResult>();

  EXPECT_CALL(*mock_get_key_value_set_result, GetValueSet("key1"))
      .WillOnce(
          Return(absl::flat_hash_set<std::string_view>{"value3", "value4"}));
  absl::flat_hash_set<std::string_view> key_set = {"key1"};
  EXPECT_CALL(mock_cache_, GetKeyValueSet(key_set))
      .WillOnce(Return(std::move(mock_get_key_value_set_result)));
  auto response_status =
      remote_lookup_client_->GetValues(serialized_message, padding_length);
  EXPECT_TRUE(response_status.ok());

  InternalLookupResponse response = *response_status;
  EXPECT_EQ(1, response.mutable_kv_pairs()->size());
  std::vector<std::string> resulting_set((*response.mutable_kv_pairs())["key1"]
                                             .mutable_keyset_values()
                                             ->mutable_values()
                                             ->begin(),
                                         (*response.mutable_kv_pairs())["key1"]
                                             .mutable_keyset_values()
                                             ->mutable_values()
                                             ->end());
  std::vector<std::string> expected_resulting_set = {"value3", "value4"};
  EXPECT_THAT(resulting_set,
              testing::UnorderedElementsAreArray(expected_resulting_set));
}

TEST_F(RemoteLookupClientImplTest,
       EncryptedPaddedSuccessfulEmptyKeysettLookup) {
  std::vector<std::string> keys = {};
  InternalLookupRequest request;
  request.mutable_keys()->Assign(keys.begin(), keys.end());
  request.set_lookup_sets(true);
  std::string serialized_message = request.SerializeAsString();
  int32_t padding_length = 10;
  auto response_status =
      remote_lookup_client_->GetValues(serialized_message, padding_length);
  EXPECT_TRUE(response_status.ok());

  InternalLookupResponse response = *response_status;
  EXPECT_EQ(0, response.mutable_kv_pairs()->size());
}

}  // namespace
}  // namespace kv_server
