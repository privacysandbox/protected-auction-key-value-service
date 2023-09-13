
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

#include "components/internal_server/sharded_lookup_server_impl.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/mocks.h"
#include "components/internal_server/mocks.h"
#include "components/sharding/mocks.h"
#include "glog/logging.h"
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

class ShardedLookupServiceImplTest : public ::testing::Test {
 protected:
  MockLookup mock_lookup_;
  std::unique_ptr<ShardedLookupServiceImpl> lookup_service_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<InternalLookupService::Stub> stub_;
};

TEST_F(ShardedLookupServiceImplTest, InternalLookup_ReturnsLookupResponse) {
  InternalLookupRequest request;
  request.add_keys("key1");

  InternalLookupResponse sharded_lookup_response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value2" }
                                   }
                              )pb",
                              &sharded_lookup_response);
  EXPECT_CALL(mock_lookup_, GetKeyValues(_))
      .WillOnce(Return(sharded_lookup_response));

  InternalLookupResponse response;
  grpc::ClientContext context;

  lookup_service_ = std::make_unique<ShardedLookupServiceImpl>(mock_lookup_);
  grpc::ServerBuilder builder;
  builder.RegisterService(lookup_service_.get());
  server_ = (builder.BuildAndStart());
  stub_ = InternalLookupService::NewStub(
      server_->InProcessChannel(grpc::ChannelArguments()));

  grpc::Status status = stub_->InternalLookup(&context, request, &response);

  EXPECT_THAT(response, EqualsProto(sharded_lookup_response));
  server_->Shutdown();
  server_->Wait();
}

TEST_F(ShardedLookupServiceImplTest,
       InternalLookup_ReturnsLookupResponseError) {
  InternalLookupRequest request;
  request.add_keys("key1");

  EXPECT_CALL(mock_lookup_, GetKeyValues(_))
      .WillOnce(Return(absl::UnknownError("some error")));

  InternalLookupResponse response;
  grpc::ClientContext context;

  lookup_service_ = std::make_unique<ShardedLookupServiceImpl>(mock_lookup_);
  grpc::ServerBuilder builder;
  builder.RegisterService(lookup_service_.get());
  server_ = (builder.BuildAndStart());
  stub_ = InternalLookupService::NewStub(
      server_->InProcessChannel(grpc::ChannelArguments()));

  grpc::Status status = stub_->InternalLookup(&context, request, &response);

  EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
  server_->Shutdown();
  server_->Wait();
}

TEST_F(ShardedLookupServiceImplTest, RunQuery_ReturnsLookupResponse) {
  InternalRunQueryRequest request;
  request.set_query("set1");

  InternalRunQueryResponse sharded_lookup_response;
  TextFormat::ParseFromString(R"pb(elements: "key1")pb",
                              &sharded_lookup_response);
  EXPECT_CALL(mock_lookup_, RunQuery(_))
      .WillOnce(Return(sharded_lookup_response));

  InternalRunQueryResponse response;
  grpc::ClientContext context;

  lookup_service_ = std::make_unique<ShardedLookupServiceImpl>(mock_lookup_);
  grpc::ServerBuilder builder;
  builder.RegisterService(lookup_service_.get());
  server_ = (builder.BuildAndStart());
  stub_ = InternalLookupService::NewStub(
      server_->InProcessChannel(grpc::ChannelArguments()));

  grpc::Status status = stub_->InternalRunQuery(&context, request, &response);

  EXPECT_THAT(response, EqualsProto(sharded_lookup_response));
  server_->Shutdown();
  server_->Wait();
}

TEST_F(ShardedLookupServiceImplTest, RunQuery_ReturnsLookupResponseError) {
  InternalRunQueryRequest request;
  request.set_query("set1");

  EXPECT_CALL(mock_lookup_, RunQuery(_))
      .WillOnce(Return(absl::UnknownError("some error")));

  InternalRunQueryResponse response;
  grpc::ClientContext context;

  lookup_service_ = std::make_unique<ShardedLookupServiceImpl>(mock_lookup_);
  grpc::ServerBuilder builder;
  builder.RegisterService(lookup_service_.get());
  server_ = (builder.BuildAndStart());
  stub_ = InternalLookupService::NewStub(
      server_->InProcessChannel(grpc::ChannelArguments()));

  grpc::Status status = stub_->InternalRunQuery(&context, request, &response);

  EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
  server_->Shutdown();
  server_->Wait();
}

}  // namespace
}  // namespace kv_server
