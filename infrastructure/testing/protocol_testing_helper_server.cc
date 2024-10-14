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
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "components/data/converters/cbor_converter.h"
#include "grpcpp/ext/proto_server_reflection_plugin.h"
#include "grpcpp/grpcpp.h"
#include "infrastructure/testing/protocol_testing_helper_server.grpc.pb.h"
#include "public/constants.h"
#include "quiche/oblivious_http/oblivious_http_client.h"
#include "src/communication/encoding_utils.h"
#include "src/communication/framing_utils.h"

ABSL_FLAG(uint16_t, port, 50050,
          "Port the server is listening on. Defaults to 50050.");

namespace kv_server {

namespace {
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;

absl::Status CborDecodeToGetValuesResponseJsonString(
    std::string_view cbor_raw, std::string& json_response) {
  nlohmann::json json_from_cbor = nlohmann::json::from_cbor(
      cbor_raw, /*strict=*/true, /*allow_exceptions=*/false);
  if (json_from_cbor.is_discarded()) {
    return absl::InternalError("Failed to convert raw CBOR buffer to JSON");
  }
  for (auto& json_compression_group : json_from_cbor["compressionGroups"]) {
    // Convert CBOR serialized list to a JSON list of partition outputs
    auto json_partition_outputs =
        GetPartitionOutputsInJson(json_compression_group["content"]);
    if (!json_partition_outputs.ok()) {
      LOG(ERROR) << json_partition_outputs.status();
      return json_partition_outputs.status();
    }
    LOG(INFO) << "json_partition_outputs" << json_partition_outputs->dump();
    json_compression_group["content"] = json_partition_outputs->dump();
  }
  json_response = json_from_cbor.dump();
  return absl::OkStatus();
}
}  // namespace

class ProtocolTestingHelperServiceImpl final
    : public ProtocolTestingHelper::Service {
  grpc::Status GetTestConfig(ServerContext* context,
                             const GetTestConfigRequest* request,
                             GetTestConfigResponse* response) override {
    response->set_public_key(public_key_);
    return grpc::Status::OK;
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
    const auto maybe_cbor_body =
        V2GetValuesRequestJsonStringCborEncode(request->body());
    if (!maybe_cbor_body.ok()) {
      LOG(ERROR) << "Converting JSON to CBOR failed: "
                 << maybe_cbor_body.status().message();
      return grpc::Status(grpc::INTERNAL,
                          std::string(maybe_cbor_body.status().message()));
    }
    auto encoded_data_size = privacy_sandbox::server_common::GetEncodedDataSize(
        maybe_cbor_body->size(), kMinResponsePaddingBytes);
    auto maybe_padded_request =
        privacy_sandbox::server_common::EncodeResponsePayload(
            privacy_sandbox::server_common::CompressionType::kUncompressed,
            std::move(*maybe_cbor_body), encoded_data_size);
    if (!maybe_padded_request.ok()) {
      LOG(ERROR) << "Padding failed: "
                 << maybe_padded_request.status().message();
      return grpc::Status(grpc::INTERNAL,
                          std::string(maybe_padded_request.status().message()));
    }
    auto encrypted_req =
        quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
            std::move(*maybe_padded_request), request->public_key(),
            *std::move(maybe_config), kKVOhttpRequestLabel);
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
      auto decrypted_response =
          quiche::ObliviousHttpResponse::CreateClientObliviousResponse(
              request->ohttp_response(), context, kKVOhttpResponseLabel);
      if (!decrypted_response.ok()) {
        LOG(ERROR) << decrypted_response.status();
      }

      auto deframed_req = privacy_sandbox::server_common::DecodeRequestPayload(
          std::move(*decrypted_response).ConsumePlaintextData());
      if (!deframed_req.ok()) {
        LOG(ERROR) << "unpadding response failed!";
        return grpc::Status(grpc::INTERNAL,
                            std::string(deframed_req.status().message()));
      }

      std::string resp_json_string;
      CborDecodeToGetValuesResponseJsonString(deframed_req->compressed_data,
                                              resp_json_string);

      response->set_body(resp_json_string);
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
