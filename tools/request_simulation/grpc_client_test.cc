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

#include "tools/request_simulation/grpc_client.h"

#include <memory>

#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "public/testing/fake_key_value_service_impl.h"
#include "tools/request_simulation/request/raw_request.pb.h"

namespace kv_server {

class GrpcClientTest : public ::testing::Test {
 protected:
  GrpcClientTest() {
    absl::flat_hash_map<std::string, std::string> data_map = {{"key", "value"}};
    fake_get_value_service_ =
        std::make_unique<FakeKeyValueServiceImpl>(data_map);
    grpc::ServerBuilder builder;
    builder.RegisterService(fake_get_value_service_.get());
    server_ = (builder.BuildAndStart());
    std::shared_ptr<grpc::Channel> channel =
        server_->InProcessChannel(grpc::ChannelArguments());
    request_converter_ = [](const std::string& request_body) {
      RawRequest request;
      request.mutable_raw_body()->set_data(request_body);
      return request;
    };
    grpc_client_ =
        std::make_unique<GrpcClient<RawRequest, google::api::HttpBody>>(
            channel, absl::Seconds(1), false);
  }

  ~GrpcClientTest() override {
    server_->Shutdown();
    server_->Wait();
  }
  std::unique_ptr<FakeKeyValueServiceImpl> fake_get_value_service_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<GrpcClient<RawRequest, google::api::HttpBody>> grpc_client_;
  absl::AnyInvocable<RawRequest(std::string)> request_converter_;
};

TEST_F(GrpcClientTest, TestRequestOKResponse) {
  std::string key("key");
  std::string method("/kv_server.v2.KeyValueService/GetValuesHttp");
  auto response_ptr = std::make_shared<google::api::HttpBody>();
  auto request_ptr = std::make_shared<RawRequest>(request_converter_(key));
  auto response = grpc_client_->SendMessage(request_ptr, method, response_ptr);
  EXPECT_TRUE(response.ok());
  EXPECT_EQ(response_ptr->data(), "value");
}

TEST_F(GrpcClientTest, TestRequestErrorResponse) {
  std::string key("missing");
  std::string method("/kv_server.v2.KeyValueService/GetValuesHttp");
  auto response_ptr = std::make_shared<google::api::HttpBody>();
  auto request_ptr = std::make_shared<RawRequest>(request_converter_(key));
  auto response = grpc_client_->SendMessage(request_ptr, method, response_ptr);
  EXPECT_FALSE(response.ok());
}

}  // namespace kv_server
