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

#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "glog/logging.h"
#include "grpcpp/ext/proto_server_reflection_plugin.h"
#include "grpcpp/grpcpp.h"
#include "infrastructure/testing/protocol_testing_helper_server.grpc.pb.h"
#include "public/constants.h"
#include "quiche/binary_http/binary_http_message.h"
#include "quiche/oblivious_http/oblivious_http_client.h"

ABSL_FLAG(uint16_t, port, 50050,
          "Port the server is listening on. Defaults to 50050.");

namespace kv_server {

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;

class ProtocolTestingHelperServiceImpl final
    : public ProtocolTestingHelper::Service {
  grpc::Status GetTestConfig(ServerContext* context,
                             const GetTestConfigRequest* request,
                             GetTestConfigResponse* response) override {
    response->set_public_key(public_key_);
    return grpc::Status::OK;
  }

  grpc::Status BHTTPEncapsulate(ServerContext* context,
                                const BHTTPEncapsulateRequest* request,
                                BHTTPEncapsulateResponse* response) override {
    const auto process = [&request, &response](auto&& bhttp_layer) {
      bhttp_layer.set_body(request->body());
      auto maybe_serialized = bhttp_layer.Serialize();
      if (!maybe_serialized.ok()) {
        return grpc::Status(grpc::INTERNAL,
                            std::string(maybe_serialized.status().message()));
      }
      response->set_bhttp_message(*maybe_serialized);
      return grpc::Status::OK;
    };
    if (request->is_request()) {
      return process(quiche::BinaryHttpRequest({}));
    }
    return process(quiche::BinaryHttpResponse(200));
  }

  grpc::Status BHTTPDecapsulate(ServerContext* context,
                                const BHTTPDecapsulateRequest* request,
                                BHTTPDecapsulateResponse* response) override {
    const auto process = [&request, &response](auto&& maybe_bhttp_layer) {
      if (!maybe_bhttp_layer.ok()) {
        return grpc::Status(grpc::INTERNAL,
                            std::string(maybe_bhttp_layer.status().message()));
      }
      std::string body;
      maybe_bhttp_layer->swap_body(body);
      response->set_body(std::move(body));
      return grpc::Status::OK;
    };

    if (request->is_request()) {
      return process(
          quiche::BinaryHttpRequest::Create(request->bhttp_message()));
    }
    return process(
        quiche::BinaryHttpResponse::Create(request->bhttp_message()));
  }

  grpc::Status OHTTPEncapsulate(ServerContext* context,
                                const OHTTPEncapsulateRequest* request,
                                OHTTPEncapsulateResponse* response) override {
    const uint8_t key_id = request->key_id();
    auto maybe_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
        key_id, kKEMParameter, kKDFParameter, kAEADParameter);
    if (!maybe_config.ok()) {
      return grpc::Status(grpc::INTERNAL,
                          std::string(maybe_config.status().message()));
    }

    auto client = quiche::ObliviousHttpClient::Create(request->public_key(),
                                                      *maybe_config);
    if (!client.ok()) {
      return grpc::Status(grpc::INTERNAL,
                          std::string(client.status().message()));
    }

    auto encrypted_req = client->CreateObliviousHttpRequest(request->body());
    if (!encrypted_req.ok()) {
      return grpc::Status(grpc::INTERNAL,
                          std::string(encrypted_req.status().message()));
    }

    auto serialized_encrypted_req = encrypted_req->EncapsulateAndSerialize();
    response->set_ohttp_request(std::move(serialized_encrypted_req));
    int64_t context_token = absl::ToUnixSeconds(absl::Now());
    response->set_context_token(context_token);
    context_map_.emplace(context_token,
                         std::move(encrypted_req.value()).ReleaseContext());
    return grpc::Status::OK;
  }

  grpc::Status OHTTPDecapsulate(ServerContext* context,
                                const OHTTPDecapsulateRequest* request,
                                OHTTPDecapsulateResponse* response) override {
    uint8_t key_id = 1;
    auto maybe_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
        key_id, kKEMParameter, kKDFParameter, kAEADParameter);
    if (!maybe_config.ok()) {
      return grpc::Status(grpc::INTERNAL,
                          std::string(maybe_config.status().message()));
    }

    auto client =
        quiche::ObliviousHttpClient::Create(public_key_, *maybe_config);
    if (!client.ok()) {
      return grpc::Status(grpc::INTERNAL,
                          std::string(client.status().message()));
    }
    if (auto context_iter = context_map_.find(request->context_token());
        context_iter == context_map_.end()) {
      return grpc::Status(
          grpc::INTERNAL,
          absl::StrCat("The context token ", request->context_token(),
                       " is not known. Did you get it from "
                       "an earlier ohttp request encapsulation call? Also, "
                       "this server loses this knowledge if it restarts."));

    } else {
      quiche::ObliviousHttpRequest::Context context =
          std::move(context_iter->second);
      context_map_.erase(context_iter);
      auto decrypted_response = client->DecryptObliviousHttpResponse(
          request->ohttp_response(), context);
      response->set_body(decrypted_response->GetPlaintextData());
      return grpc::Status::OK;
    }
  }

  std::unordered_map<int64_t, quiche::ObliviousHttpRequest::Context>
      context_map_;
  const std::string public_key_ = absl::HexStringToBytes(kTestPublicKey);
};

void RunServer() {
  std::string server_address(
      absl::StrCat("0.0.0.0:", absl::GetFlag(FLAGS_port)));
  ProtocolTestingHelperServiceImpl service;

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}
}  // namespace kv_server

int main(int argc, char** argv) {
  kv_server::RunServer();

  return 0;
}
