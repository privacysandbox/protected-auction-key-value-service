
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

#include "components/internal_server/lookup_server_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/key_value_cache.h"
#include "components/data_server/cache/mocks.h"
#include "components/internal_server/string_padder.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "public/test_util/proto_matcher.h"
#include "src/cpp/encryption/key_fetcher/src/fake_key_fetcher_manager.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using privacy_sandbox::server_common::MockMetricsRecorder;
using testing::_;
using testing::Return;
using testing::ReturnRef;

class LookupServiceImplTest : public ::testing::Test {
 protected:
  LookupServiceImplTest() {
    lookup_service_ = std::make_unique<LookupServiceImpl>(
        mock_cache_, fake_key_fetcher_manager_, mock_metrics_recorder_);
    grpc::ServerBuilder builder;
    builder.RegisterService(lookup_service_.get());
    server_ = (builder.BuildAndStart());

    stub_ = InternalLookupService::NewStub(
        server_->InProcessChannel(grpc::ChannelArguments()));
  }

  ~LookupServiceImplTest() {
    server_->Shutdown();
    server_->Wait();
  }
  MockCache mock_cache_;
  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager_;
  std::unique_ptr<LookupServiceImpl> lookup_service_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<InternalLookupService::Stub> stub_;
  MockMetricsRecorder mock_metrics_recorder_;
};

TEST_F(LookupServiceImplTest, ReturnsKeysFromCache) {
  InternalLookupRequest request;
  request.add_keys("key1");
  request.add_keys("key2");
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(_))
      .WillOnce(Return(absl::flat_hash_map<std::string, std::string>{
          {"key1", "value1"}, {"key2", "value2"}}));

  InternalLookupResponse response;
  grpc::ClientContext context;

  grpc::Status status = stub_->InternalLookup(&context, request, &response);

  InternalLookupResponse expected;
  TextFormat::ParseFromString(R"pb(kv_pairs {
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

TEST_F(LookupServiceImplTest, MissingKeyFromCache) {
  InternalLookupRequest request;
  request.add_keys("key1");
  request.add_keys("key2");
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(_))
      .WillOnce(Return(
          absl::flat_hash_map<std::string, std::string>{{"key1", "value1"}}));

  InternalLookupResponse response;
  grpc::ClientContext context;

  grpc::Status status = stub_->InternalLookup(&context, request, &response);

  InternalLookupResponse expected;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { value: "value1" }
           }
           kv_pairs {
             key: "key2"
             value { status: { code: 5, message: "" } }
           }
      )pb",
      &expected);
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST_F(LookupServiceImplTest, InternalRunQuerySuccess) {
  InternalRunQueryRequest request;
  request.set_query("someset");

  absl::flat_hash_set<std::string_view> keys;
  keys.emplace("someset");
  auto mock_get_key_value_set_result =
      std::make_unique<MockGetKeyValueSetResult>();
  EXPECT_CALL(*mock_get_key_value_set_result, GetValueSet(_))
      .WillOnce(
          Return(absl::flat_hash_set<std::string_view>{"value1", "value2"}));
  EXPECT_CALL(mock_cache_, GetKeyValueSet(_))
      .WillOnce(Return(std::move(mock_get_key_value_set_result)));
  InternalRunQueryResponse response;
  grpc::ClientContext context;
  grpc::Status status = stub_->InternalRunQuery(&context, request, &response);
  auto results = response.elements();
  EXPECT_THAT(results,
              testing::UnorderedElementsAreArray({"value1", "value2"}));
}

TEST_F(LookupServiceImplTest, InternalRunQueryParseFailure) {
  InternalRunQueryRequest request;
  request.set_query("fail|||||now");
  InternalRunQueryResponse response;
  grpc::ClientContext context;
  grpc::Status status = stub_->InternalRunQuery(&context, request, &response);
  auto results = response.elements();
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
}

TEST_F(LookupServiceImplTest, SecureLookupFailure) {
  SecureLookupRequest secure_lookup_request;
  secure_lookup_request.set_ohttp_request("garbage");
  SecureLookupResponse response;
  grpc::ClientContext context;
  grpc::Status status =
      stub_->SecureLookup(&context, secure_lookup_request, &response);
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
}

}  // namespace

}  // namespace kv_server
