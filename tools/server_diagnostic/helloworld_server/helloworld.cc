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

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "grpcpp/ext/proto_server_reflection_plugin.h"
#include "grpcpp/grpcpp.h"
#include "public/query/get_values.grpc.pb.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"

ABSL_FLAG(uint16_t, port, 50051,
          "Port the server is listening on. Defaults to 50051.");

// A simple helloworld grpc server serving KV V1 and V2 requests.
// This server will be used to test in isolation any system dependency
// such as envoy proxy that could affect KV server inbound/outbound
// communication.

// Implements Key-Value service Query V2 API.
class HelloWorldV2Impl final
    : public kv_server::v2::KeyValueService::CallbackService {
 public:
  grpc::ServerUnaryReactor* GetValuesHttp(
      grpc::CallbackServerContext* context,
      const kv_server::v2::GetValuesHttpRequest* request,
      google::api::HttpBody* response) override {
    auto request_body = request->raw_body().data();
    LOG(INFO) << "Received v2 request " << request_body;
    response->set_data("Hello, received V2 request " + request_body);
    auto* reactor = context->DefaultReactor();
    reactor->Finish(grpc::Status::OK);
    LOG(INFO) << "Response is sent!";
    return reactor;
  }
};

// Implements Key-Value service Query V1 API.
class HelloWorldV1Impl final
    : public kv_server::v1::KeyValueService::CallbackService {
 public:
  grpc::ServerUnaryReactor* GetValues(
      grpc::CallbackServerContext* context,
      const kv_server::v1::GetValuesRequest* request,
      kv_server::v1::GetValuesResponse* response) override {
    LOG(INFO) << "Received v1 request " << request->DebugString();
    auto data_map = response->mutable_kv_internal();
    google::protobuf::Value value;
    value.set_string_value("Hello, received v1 request " +
                           request->DebugString());
    *(*data_map)["request"].mutable_value() = value;
    auto* reactor = context->DefaultReactor();
    reactor->Finish(grpc::Status::OK);
    LOG(INFO) << "Response is sent!";
    return reactor;
  }
};
void CreateAndRunServer() {
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  grpc::ServerBuilder builder;
  const std::string server_address =
      absl::StrCat("0.0.0.0:", absl::GetFlag(FLAGS_port));
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  HelloWorldV1Impl v1_api_service;
  HelloWorldV2Impl v2_api_service;

  builder.RegisterService(&v1_api_service);
  builder.RegisterService(&v2_api_service);

  LOG(INFO) << "Server listening on " << server_address << std::endl;
  auto server = builder.BuildAndStart();
  server->Wait();
}

int main(int argc, char** argv) {
  CreateAndRunServer();
  return 0;
}
