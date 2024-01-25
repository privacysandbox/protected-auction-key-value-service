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

#include "components/data_server/request_handler/get_values_v2_handler.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "components/data_server/request_handler/ohttp_server_encryptor.h"
#include "components/telemetry/server_definition.h"
#include "google/protobuf/util/json_util.h"
#include "grpcpp/grpcpp.h"
#include "public/base_types.pb.h"
#include "public/constants.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"
#include "quiche/binary_http/binary_http_message.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "quiche/oblivious_http/oblivious_http_gateway.h"
#include "src/cpp/telemetry/telemetry.h"
#include "src/cpp/util/status_macro/status_macros.h"

constexpr char* kCacheKeyV2Hit = "CacheKeyHit";
constexpr char* kCacheKeyV2Miss = "CacheKeyMiss";

namespace kv_server {
namespace {
using google::protobuf::util::JsonStringToMessage;
using google::protobuf::util::MessageToJsonString;
using grpc::StatusCode;
using privacy_sandbox::server_common::FromAbslStatus;
using v2::GetValuesHttpRequest;
using v2::KeyValueService;
using v2::ObliviousGetValuesRequest;

const std::string_view kOHTTPResponseContentType = "message/ohttp-res";
constexpr std::string_view kAcceptEncodingHeader = "accept-encoding";
constexpr std::string_view kContentEncodingHeader = "content-encoding";
constexpr std::string_view kBrotliAlgorithmHeader = "br";

CompressionGroupConcatenator::CompressionType GetResponseCompressionType(
    const std::vector<quiche::BinaryHttpMessage::Field>& headers) {
  for (const quiche::BinaryHttpMessage::Field& header : headers) {
    if (absl::AsciiStrToLower(header.name) != kAcceptEncodingHeader) continue;
    // TODO(b/278271389): Right now for simplicity we support Accept-Encoding:
    // br
    if (absl::AsciiStrToLower(header.value) == kBrotliAlgorithmHeader) {
      return CompressionGroupConcatenator::CompressionType::kBrotli;
    }
  }
  return CompressionGroupConcatenator::CompressionType::kUncompressed;
}
}  // namespace

grpc::Status GetValuesV2Handler::GetValuesHttp(
    const GetValuesHttpRequest& request,
    google::api::HttpBody* response) const {
  return FromAbslStatus(
      GetValuesHttp(request.raw_body().data(), *response->mutable_data()));
}

absl::Status GetValuesV2Handler::GetValuesHttp(
    std::string_view request, std::string& json_response) const {
  v2::GetValuesRequest request_proto;
  PS_RETURN_IF_ERROR(
      google::protobuf::util::JsonStringToMessage(request, &request_proto));
  VLOG(9) << "Converted the http request to proto: "
          << request_proto.DebugString();
  v2::GetValuesResponse response_proto;
  PS_RETURN_IF_ERROR(GetValues(request_proto, &response_proto));
  return MessageToJsonString(response_proto, &json_response);
}

grpc::Status GetValuesV2Handler::BinaryHttpGetValues(
    const v2::BinaryHttpGetValuesRequest& bhttp_request,
    google::api::HttpBody* response) const {
  return FromAbslStatus(BinaryHttpGetValues(bhttp_request.raw_body().data(),
                                            *response->mutable_data()));
}

absl::StatusOr<quiche::BinaryHttpResponse>
GetValuesV2Handler::BuildSuccessfulGetValuesBhttpResponse(
    std::string_view bhttp_request_body) const {
  VLOG(9) << "Handling the binary http layer";
  PS_ASSIGN_OR_RETURN(quiche::BinaryHttpRequest maybe_deserialized_req,
                      quiche::BinaryHttpRequest::Create(bhttp_request_body),
                      _ << "Failed to deserialize binary http request");
  VLOG(3) << "BinaryHttpGetValues request: "
          << maybe_deserialized_req.DebugString();

  std::string json_response;
  PS_RETURN_IF_ERROR(
      GetValuesHttp(maybe_deserialized_req.body(), json_response));

  quiche::BinaryHttpResponse bhttp_response(200);
  bhttp_response.set_body(std::move(json_response));
  return bhttp_response;
}

absl::Status GetValuesV2Handler::BinaryHttpGetValues(
    std::string_view bhttp_request_body, std::string& response) const {
  static quiche::BinaryHttpResponse const* kDefaultBhttpResponse =
      new quiche::BinaryHttpResponse(500);
  const quiche::BinaryHttpResponse* bhttp_response = kDefaultBhttpResponse;
  absl::StatusOr<quiche::BinaryHttpResponse> maybe_successful_bhttp_response =
      BuildSuccessfulGetValuesBhttpResponse(bhttp_request_body);
  if (maybe_successful_bhttp_response.ok()) {
    bhttp_response = &(maybe_successful_bhttp_response.value());
  }
  PS_ASSIGN_OR_RETURN(auto serialized_bhttp_response,
                      bhttp_response->Serialize());

  response = std::move(serialized_bhttp_response);
  VLOG(9) << "BinaryHttpGetValues finished successfully";
  return absl::OkStatus();
}

grpc::Status GetValuesV2Handler::ObliviousGetValues(
    const ObliviousGetValuesRequest& oblivious_request,
    google::api::HttpBody* oblivious_response) const {
  VLOG(9) << "Received ObliviousGetValues request. ";
  OhttpServerEncryptor encryptor(key_fetcher_manager_);
  auto maybe_plain_text =
      encryptor.DecryptRequest(oblivious_request.raw_body().data());
  if (!maybe_plain_text.ok()) {
    return FromAbslStatus(maybe_plain_text.status());
  }
  // Now process the binary http request
  std::string response;
  if (const auto s = BinaryHttpGetValues(*maybe_plain_text, response);
      !s.ok()) {
    return FromAbslStatus(s);
  }
  auto encrypted_response = encryptor.EncryptResponse(std::move(response));
  if (!encrypted_response.ok()) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        absl::StrCat(encrypted_response.status().code(), " : ",
                                     encrypted_response.status().message()));
  }
  oblivious_response->set_content_type(std::string(kOHTTPResponseContentType));
  oblivious_response->set_data(*encrypted_response);
  return grpc::Status::OK;
}

void GetValuesV2Handler::ProcessOnePartition(
    RequestContext request_context,
    const google::protobuf::Struct& req_metadata,
    const v2::RequestPartition& req_partition,
    v2::ResponsePartition& resp_partition) const {
  resp_partition.set_id(req_partition.id());
  UDFExecutionMetadata udf_metadata;
  *udf_metadata.mutable_request_metadata() = req_metadata;
  const auto maybe_output_string = udf_client_.ExecuteCode(
      std::move(request_context), std::move(udf_metadata),
      req_partition.arguments());
  if (!maybe_output_string.ok()) {
    resp_partition.mutable_status()->set_code(
        static_cast<int>(maybe_output_string.status().code()));
    resp_partition.mutable_status()->set_message(
        maybe_output_string.status().message());
  } else {
    VLOG(5) << "UDF output: " << maybe_output_string.value();
    resp_partition.set_string_output(std::move(maybe_output_string).value());
  }
}

grpc::Status GetValuesV2Handler::GetValues(
    const v2::GetValuesRequest& request,
    v2::GetValuesResponse* response) const {
  auto scope_metrics_context = std::make_unique<ScopeMetricsContext>();
  RequestContext request_context(scope_metrics_context->GetMetricsContext());
  if (request.partitions().size() == 1) {
    ProcessOnePartition(std::move(request_context), request.metadata(),
                        request.partitions(0),
                        *response->mutable_single_partition());
    return grpc::Status::OK;
  }
  if (request.partitions().empty()) {
    return grpc::Status(StatusCode::INTERNAL,
                        "At least 1 partition is required");
  }
  return grpc::Status(StatusCode::UNIMPLEMENTED,
                      "Multiple partition support is not implemented");
}

}  // namespace kv_server
