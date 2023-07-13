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

namespace kv_server {
namespace {

class RemoteLookupClientImplTest : public ::testing::Test {
 protected:
  RemoteLookupClientImplTest() {
    lookup_service_ = std::make_unique<LookupServiceImpl>(mock_cache_);
    grpc::ServerBuilder builder;
    builder.RegisterService(lookup_service_.get());
    server_ = (builder.BuildAndStart());
    remote_lookup_client_ =
        RemoteLookupClient::Create(InternalLookupService::NewStub(
            server_->InProcessChannel(grpc::ChannelArguments())));
  }

  ~RemoteLookupClientImplTest() {
    server_->Shutdown();
    server_->Wait();
  }
  MockCache mock_cache_;
  std::unique_ptr<LookupServiceImpl> lookup_service_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<RemoteLookupClient> remote_lookup_client_;
};

TEST_F(RemoteLookupClientImplTest, EncryptedPaddedSuccessfulCall) {
  std::vector<std::string> keys = {"key1", "key2"};
  InternalLookupRequest request;
  request.mutable_keys()->Assign(keys.begin(), keys.end());
  std::string serialized_message = request.SerializeAsString();
  InternalLookupRequest request2;
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

}  // namespace
}  // namespace kv_server
