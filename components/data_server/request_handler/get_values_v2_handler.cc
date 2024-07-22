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
#include "components/data_server/request_handler/framing_utils.h"
#include "components/data_server/request_handler/get_values_v2_status.h"
#include "components/data_server/request_handler/ohttp_server_encryptor.h"
#include "components/telemetry/server_definition.h"
#include "google/protobuf/util/json_util.h"
#include "grpcpp/grpcpp.h"
#include "public/base_types.pb.h"
#include "public/constants.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "quiche/oblivious_http/oblivious_http_gateway.h"
#include "src/communication/encoding_utils.h"
#include "src/telemetry/telemetry.h"
#include "src/util/status_macro/status_macros.h"

namespace kv_server {
namespace {
using google::protobuf::util::JsonStringToMessage;
using google::protobuf::util::MessageToJsonString;
using grpc::StatusCode;
using privacy_sandbox::server_common::FromAbslStatus;
using v2::GetValuesHttpRequest;
using v2::KeyValueService;
using v2::ObliviousGetValuesRequest;

const std::string_view kOHTTPResponseContentType =
    "message/ad-auction-trusted-signals-response; v=2.0";
constexpr std::string_view kAcceptEncodingHeader = "accept-encoding";
constexpr std::string_view kContentEncodingHeader = "content-encoding";
constexpr std::string_view kBrotliAlgorithmHeader = "br";

}  // namespace

grpc::Status GetValuesV2Handler::GetValuesHttp(
    RequestContextFactory& request_context_factory,
    const std::multimap<grpc::string_ref, grpc::string_ref>& headers,
    const GetValuesHttpRequest& request, google::api::HttpBody* response,
    ExecutionMetadata& execution_metadata) const {
  return FromAbslStatus(
      GetValuesHttp(request_context_factory, request.raw_body().data(),
                    *response->mutable_data(), execution_metadata,
                    GetContentType(headers, ContentType::kJson)));
}

absl::Status GetValuesV2Handler::GetValuesHttp(
    RequestContextFactory& request_context_factory, std::string_view request,
    std::string& response, ExecutionMetadata& execution_metadata,
    ContentType content_type) const {
  v2::GetValuesRequest request_proto;
  // TODO (b/353537363): implement CBOR
  if (content_type == ContentType::kJson ||
      content_type == ContentType::kCbor) {
    PS_RETURN_IF_ERROR(
        google::protobuf::util::JsonStringToMessage(request, &request_proto));
  } else {  // proto
    if (!request_proto.ParseFromString(request)) {
      auto error_message =
          "Cannot parse request as a valid serilized proto object.";
      PS_VLOG(4, request_context_factory.Get().GetPSLogContext())
          << error_message;
      return absl::InvalidArgumentError(error_message);
    }
  }
  PS_VLOG(9) << "Converted the http request to proto: "
             << request_proto.DebugString();
  v2::GetValuesResponse response_proto;
  PS_RETURN_IF_ERROR(GetValues(request_context_factory, request_proto,
                               &response_proto, execution_metadata));
  // TODO (b/353537363): implement CBOR
  if (content_type == ContentType::kJson ||
      content_type == ContentType::kCbor) {
    return MessageToJsonString(response_proto, &response);
  }
  // content_type == proto
  if (!response_proto.SerializeToString(&response)) {
    auto error_message = "Cannot serialize the response as a proto.";
    PS_VLOG(4, request_context_factory.Get().GetPSLogContext())
        << error_message;
    return absl::InvalidArgumentError(error_message);
  }
  return absl::OkStatus();
}

GetValuesV2Handler::ContentType GetValuesV2Handler::GetContentType(
    const std::multimap<grpc::string_ref, grpc::string_ref>& headers,
    ContentType default_content_type) const {
  for (const auto& [header_name, header_value] : headers) {
    if (absl::AsciiStrToLower(std::string_view(
            header_name.data(), header_name.size())) == kKVContentTypeHeader) {
      if (absl::AsciiStrToLower(
              std::string_view(header_value.data(), header_value.size())) ==
          kContentEncodingProtoHeaderValue) {
        return ContentType::kProto;
      } else if (absl::AsciiStrToLower(std::string_view(header_value.data(),
                                                        header_value.size())) ==
                 kContentEncodingJsonHeaderValue) {
        return ContentType::kJson;
      }
    }
  }
  return default_content_type;
}

grpc::Status GetValuesV2Handler::ObliviousGetValues(
    RequestContextFactory& request_context_factory,
    const std::multimap<grpc::string_ref, grpc::string_ref>& headers,
    const ObliviousGetValuesRequest& oblivious_request,
    google::api::HttpBody* oblivious_response,
    ExecutionMetadata& execution_metadata) const {
  PS_VLOG(9) << "Received ObliviousGetValues request. ";
  OhttpServerEncryptor encryptor(key_fetcher_manager_);
  auto maybe_padded_plain_text =
      encryptor.DecryptRequest(oblivious_request.raw_body().data(),
                               request_context_factory.Get().GetPSLogContext());
  if (!maybe_padded_plain_text.ok()) {
    return FromAbslStatus(maybe_padded_plain_text.status());
  }
  std::string response;
  absl::StatusOr<privacy_sandbox::server_common::DecodedRequest>
      decoded_request = privacy_sandbox::server_common::DecodeRequestPayload(
          *maybe_padded_plain_text);
  if (!decoded_request.ok()) {
    return FromAbslStatus(decoded_request.status());
  }
  auto content_type = GetContentType(headers, ContentType::kCbor);
  if (const auto s = GetValuesHttp(request_context_factory,
                                   std::move(decoded_request->compressed_data),
                                   response, execution_metadata, content_type);
      !s.ok()) {
    return FromAbslStatus(s);
  }
  auto encoded_data_size = GetEncodedDataSize(response.size());
  auto maybe_padded_response =
      privacy_sandbox::server_common::EncodeResponsePayload(
          privacy_sandbox::server_common::CompressionType::kUncompressed,
          std::move(response), encoded_data_size);
  if (!maybe_padded_response.ok()) {
    return FromAbslStatus(maybe_padded_response.status());
  }
  auto encrypted_response = encryptor.EncryptResponse(
      std::move(*maybe_padded_response),
      request_context_factory.Get().GetPSLogContext());
  if (!encrypted_response.ok()) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        absl::StrCat(encrypted_response.status().code(), " : ",
                                     encrypted_response.status().message()));
  }
  oblivious_response->set_content_type(std::string(kOHTTPResponseContentType));
  oblivious_response->set_data(*encrypted_response);
  return grpc::Status::OK;
}

absl::Status GetValuesV2Handler::ProcessOnePartition(
    const RequestContextFactory& request_context_factory,
    const google::protobuf::Struct& req_metadata,
    const v2::RequestPartition& req_partition,
    v2::ResponsePartition& resp_partition,
    ExecutionMetadata& execution_metadata) const {
  resp_partition.set_id(req_partition.id());
  UDFExecutionMetadata udf_metadata;
  *udf_metadata.mutable_request_metadata() = req_metadata;

  const auto maybe_output_string = udf_client_.ExecuteCode(
      std::move(request_context_factory), std::move(udf_metadata),
      req_partition.arguments(), execution_metadata);
  if (!maybe_output_string.ok()) {
    resp_partition.mutable_status()->set_code(
        static_cast<int>(maybe_output_string.status().code()));
    resp_partition.mutable_status()->set_message(
        maybe_output_string.status().message());
    return maybe_output_string.status();
  }
  PS_VLOG(5, request_context_factory.Get().GetPSLogContext())
      << "UDF output: " << maybe_output_string.value();
  resp_partition.set_string_output(std::move(maybe_output_string).value());
  return absl::OkStatus();
}

grpc::Status GetValuesV2Handler::GetValues(
    RequestContextFactory& request_context_factory,
    const v2::GetValuesRequest& request, v2::GetValuesResponse* response,
    ExecutionMetadata& execution_metadata) const {
  PS_VLOG(9) << "Update log context " << request.log_context() << ";"
             << request.consented_debug_config();
  request_context_factory.UpdateLogContext(request.log_context(),
                                           request.consented_debug_config());
  if (request.partitions().size() == 1) {
    const auto partition_status = ProcessOnePartition(
        request_context_factory, request.metadata(), request.partitions(0),
        *response->mutable_single_partition(), execution_metadata);
    return GetExternalStatusForV2(partition_status);
  }
  if (request.partitions().empty()) {
    return grpc::Status(StatusCode::INTERNAL,
                        "At least 1 partition is required");
  }
  return grpc::Status(StatusCode::UNIMPLEMENTED,
                      "Multiple partition support is not implemented");
}

}  // namespace kv_server
