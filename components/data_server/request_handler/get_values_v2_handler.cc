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
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "components/data/converters/cbor_converter.h"
#include "components/data_server/request_handler/encryption/ohttp_server_encryptor.h"
#include "components/data_server/request_handler/get_values_v2_status.h"
#include "components/telemetry/server_definition.h"
#include "google/protobuf/util/json_util.h"
#include "grpcpp/grpcpp.h"
#include "nlohmann/json.hpp"
#include "public/applications/pa/response_utils.h"
#include "public/base_types.pb.h"
#include "public/constants.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "quiche/oblivious_http/oblivious_http_gateway.h"
#include "src/communication/encoding_utils.h"
#include "src/communication/framing_utils.h"
#include "src/telemetry/telemetry.h"
#include "src/util/status_macro/status_macros.h"

namespace kv_server {
namespace {

using grpc::StatusCode;
using privacy_sandbox::server_common::FromAbslStatus;
using v2::GetValuesHttpRequest;
using v2::ObliviousGetValuesRequest;

constexpr std::string_view kIsPas = "is_pas";

bool HasDuplicatePartitionIds(const v2::GetValuesRequest& request) {
  std::unordered_set<int32_t> ids;
  for (auto&& partition : request.partitions()) {
    if (ids.find(partition.id()) != ids.end()) {
      return true;
    }
    ids.insert(partition.id());
  }
  return false;
}
}  // namespace

grpc::Status GetValuesV2Handler::GetValuesHttp(
    RequestContextFactory& request_context_factory,
    const std::multimap<grpc::string_ref, grpc::string_ref>& headers,
    const GetValuesHttpRequest& request, google::api::HttpBody* response,
    ExecutionMetadata& execution_metadata) const {
  PS_VLOG(9, request_context_factory.Get().GetPSLogContext())
      << "GetValuesHttpRequest with headers: " << request;
  auto v2_codec = V2EncoderDecoder::Create(V2EncoderDecoder::GetContentType(
      headers, V2EncoderDecoder::ContentType::kJson));
  return FromAbslStatus(
      GetValuesHttp(request_context_factory, request.raw_body().data(),
                    *response->mutable_data(), execution_metadata, *v2_codec));
}

absl::Status GetValuesV2Handler::GetValuesHttp(
    RequestContextFactory& request_context_factory, std::string_view request,
    std::string& response, ExecutionMetadata& execution_metadata,
    const V2EncoderDecoder& v2_codec) const {
  PS_VLOG(9, request_context_factory.Get().GetPSLogContext())
      << "GetValuesHttpRequest body: " << request;
  PS_ASSIGN_OR_RETURN(v2::GetValuesRequest request_proto,
                      v2_codec.DecodeToV2GetValuesRequestProto(request));

  v2::GetValuesResponse response_proto;
  PS_RETURN_IF_ERROR(GetValues(
      request_context_factory, request_proto, &response_proto,
      execution_metadata, IsSinglePartitionUseCase(request_proto), v2_codec));
  PS_ASSIGN_OR_RETURN(response,
                      v2_codec.EncodeV2GetValuesResponse(response_proto));
  return absl::OkStatus();
}

bool IsSinglePartitionUseCase(const v2::GetValuesRequest& request) {
  const auto is_pas_field = request.metadata().fields().find(kIsPas);
  return (is_pas_field != request.metadata().fields().end() &&
          is_pas_field->second.string_value() == "true");
}

grpc::Status GetValuesV2Handler::ObliviousGetValues(
    RequestContextFactory& request_context_factory,
    const std::multimap<grpc::string_ref, grpc::string_ref>& headers,
    const ObliviousGetValuesRequest& oblivious_request,
    google::api::HttpBody* oblivious_response,
    ExecutionMetadata& execution_metadata) const {
  PS_VLOG(9, request_context_factory.Get().GetPSLogContext())
      << "Received ObliviousGetValues request.";
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
  auto v2_codec = V2EncoderDecoder::Create(V2EncoderDecoder::GetContentType(
      headers, V2EncoderDecoder::ContentType::kCbor));
  if (const auto s = GetValuesHttp(request_context_factory,
                                   std::move(decoded_request->compressed_data),
                                   response, execution_metadata, *v2_codec);
      !s.ok()) {
    return FromAbslStatus(s);
  }
  auto encoded_data_size = privacy_sandbox::server_common::GetEncodedDataSize(
      response.size(), kMinResponsePaddingBytes);
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
  oblivious_response->set_content_type(std::string(kKVOhttpResponseLabel));
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
  if (!req_partition.metadata().fields().empty()) {
    *udf_metadata.mutable_partition_metadata() = req_partition.metadata();
  }

  const auto maybe_output_string =
      udf_client_.ExecuteCode(request_context_factory, std::move(udf_metadata),
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

absl::Status GetValuesV2Handler::ProcessMultiplePartitions(
    const RequestContextFactory& request_context_factory,
    const v2::GetValuesRequest& request, v2::GetValuesResponse& response,
    ExecutionMetadata& execution_metadata,
    const V2EncoderDecoder& v2_codec) const {
  absl::flat_hash_map<int32_t, UDFInput> udf_input_map;
  for (const auto& partition : request.partitions()) {
    UDFExecutionMetadata udf_metadata;
    *udf_metadata.mutable_request_metadata() = request.metadata();
    if (!partition.metadata().fields().empty()) {
      *udf_metadata.mutable_partition_metadata() = partition.metadata();
    }
    udf_input_map.emplace(
        partition.id(), UDFInput{.execution_metadata = std::move(udf_metadata),
                                 .arguments = partition.arguments()});
  }
  PS_ASSIGN_OR_RETURN(
      auto partition_id_to_output_map,
      udf_client_.BatchExecuteCode(request_context_factory, udf_input_map,
                                   execution_metadata));

  absl::flat_hash_map<int32_t, std::vector<std::pair<int32_t, std::string>>>
      compression_group_map;
  for (const auto& partition : request.partitions()) {
    int32_t compression_group_id = partition.compression_group_id();
    auto it = partition_id_to_output_map.find(partition.id());
    if (it != partition_id_to_output_map.end()) {
      compression_group_map[compression_group_id].emplace_back(
          partition.id(), std::move(it->second));
    } else {
      PS_VLOG(3, request_context_factory.Get().GetPSLogContext())
          << "Failed to execute UDF for partition: " << partition.id();
    }
  }

  // The content of each compressed blob is a CBOR/JSON list of partition
  // outputs or a V2CompressionGroup protobuf message.
  for (auto& [group_id, partition_output_pairs] : compression_group_map) {
    const auto maybe_content = v2_codec.EncodePartitionOutputs(
        partition_output_pairs, request_context_factory);
    if (!maybe_content.ok()) {
      PS_VLOG(3, request_context_factory.Get().GetPSLogContext())
          << maybe_content.status();
      continue;
    }
    // TODO(b/355464083): Compress the compression_group content
    auto* compression_group = response.add_compression_groups();
    compression_group->set_content(std::move(*maybe_content));
    compression_group->set_compression_group_id(group_id);
  }
  if (response.compression_groups().empty()) {
    return absl::InvalidArgumentError("All partitions failed.");
  }
  return absl::OkStatus();
}

grpc::Status GetValuesV2Handler::GetValues(
    RequestContextFactory& request_context_factory,
    const v2::GetValuesRequest& request, v2::GetValuesResponse* response,
    ExecutionMetadata& execution_metadata, bool single_partition_use_case,
    const V2EncoderDecoder& v2_codec) const {
  PS_VLOG(9) << "Update log context " << request.log_context() << ";"
             << request.consented_debug_config();
  request_context_factory.UpdateLogContext(
      request.log_context(), request.consented_debug_config(),
      [response]() { return response->mutable_debug_info(); });
  PS_VLOG(9, request_context_factory.Get().GetPSLogContext())
      << "v2 GetValuesRequest: " << request;
  if (request.partitions().empty()) {
    return grpc::Status(StatusCode::INTERNAL,
                        "At least 1 partition is required");
  }
  if (HasDuplicatePartitionIds(request)) {
    return grpc::Status(StatusCode::INVALID_ARGUMENT,
                        "Request partition IDs must be unique.");
  }
  if (single_partition_use_case) {
    if (request.partitions().size() > 1) {
      return grpc::Status(StatusCode::UNIMPLEMENTED,
                          "This use case only accepts single partitions, but "
                          "multiple partitions were found.");
    }
    // TODO(b/355434272): Return early on CBOR content type (not supported)
    const auto response_status = ProcessOnePartition(
        request_context_factory, request.metadata(), request.partitions(0),
        *response->mutable_single_partition(), execution_metadata);
    return GetExternalStatusForV2(response_status);
  }
  const auto response_status =
      ProcessMultiplePartitions(request_context_factory, request, *response,
                                execution_metadata, v2_codec);
  return GetExternalStatusForV2(response_status);
}

}  // namespace kv_server
