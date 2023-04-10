
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

#include "components/internal_lookup/lookup_server_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "public/test_util/proto_matcher.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using testing::_;
using testing::Return;
using testing::ReturnRef;

class LookupServiceImplTest : public ::testing::Test {
 protected:
  LookupServiceImplTest() {
    lookup_service_ = std::make_unique<LookupServiceImpl>(mock_cache_);
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
  std::unique_ptr<LookupServiceImpl> lookup_service_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<InternalLookupService::Stub> stub_;
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
             value { status: { code: 5, message: "Key not found" } }
           }
      )pb",
      &expected);
  EXPECT_THAT(response, EqualsProto(expected));
}

}  // namespace

}  // namespace kv_server
