
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

#include <limits>
#include <memory>

#include "components/internal_server/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "public/test_util/proto_matcher.h"
#include "src/encryption/key_fetcher/fake_key_fetcher_manager.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using testing::_;
using testing::Return;

class LookupServiceImplTest : public ::testing::Test {
 protected:
  LookupServiceImplTest() {
    lookup_service_ = std::make_unique<LookupServiceImpl>(
        mock_lookup_, fake_key_fetcher_manager_);
    grpc::ServerBuilder builder;
    builder.RegisterService(lookup_service_.get());
    server_ = (builder.BuildAndStart());

    stub_ = InternalLookupService::NewStub(
        server_->InProcessChannel(grpc::ChannelArguments()));
    InitMetricsContextMap();
  }
  ~LookupServiceImplTest() {
    server_->Shutdown();
    server_->Wait();
  }
  MockLookup mock_lookup_;
  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager_;
  std::unique_ptr<LookupServiceImpl> lookup_service_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<InternalLookupService::Stub> stub_;
};

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
