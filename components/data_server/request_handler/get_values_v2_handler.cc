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
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "components/data_server/request_handler/ohttp_server_encryptor.h"
#include "glog/logging.h"
#include "grpcpp/grpcpp.h"
#include "public/base_types.pb.h"
#include "public/constants.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"
#include "quiche/binary_http/binary_http_message.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "quiche/oblivious_http/oblivious_http_gateway.h"
#include "src/cpp/telemetry/telemetry.h"

constexpr char* kCacheKeyV2Hit = "CacheKeyHit";
constexpr char* kCacheKeyV2Miss = "CacheKeyMiss";

constexpr int kUdfInputApiVersion = 1;
constexpr int kUdfOutputApiVersion = 1;

namespace kv_server {
namespace {
using grpc::StatusCode;
using privacy_sandbox::server_common::GetTracer;
using quiche::BinaryHttpRequest;
using quiche::BinaryHttpResponse;
using v2::BinaryHttpGetValuesRequest;
using v2::GetValuesRequest;
using v2::KeyValueService;
using v2::ObliviousGetValuesRequest;

const std::string_view kOHTTPResponseContentType = "message/ohttp-res";
constexpr std::string_view kAcceptEncodingHeader = "accept-encoding";
constexpr std::string_view kContentEncodingHeader = "content-encoding";
constexpr std::string_view kBrotliAlgorithmHeader = "br";

absl::StatusOr<nlohmann::json> ExecuteUdfForKeyGroups(
    const UdfClient& udf_client, const nlohmann::json& udf_input) {
  const auto maybe_udf_output_string =
      udf_client.ExecuteCode(std::vector<std::string>({udf_input.dump()}));
  if (!maybe_udf_output_string.ok()) {
    return maybe_udf_output_string.status();
  }
  VLOG(5) << "UDF output: " << maybe_udf_output_string.value();
  nlohmann::json key_group_outputs =
      nlohmann::json::parse(std::move(maybe_udf_output_string.value()), nullptr,
                            /*allow_exceptions=*/false,
                            /*ignore_comments=*/true);
  if (key_group_outputs.is_discarded()) {
    return absl::InvalidArgumentError("Error while parsing UDF output.");
  }
  return key_group_outputs;
}

// Processes partition, passing its keyGroups to a single UDF call.
absl::StatusOr<nlohmann::json> ProcessPartition(
    const UdfClient& udf_client, const nlohmann::json& context,
    const nlohmann::json& partition) {
  nlohmann::json partition_output = {{"id", partition["id"]}};
  auto& group_outputs = partition_output["keyGroupOutputs"];

  // Create keyGroups item for UDF input
  nlohmann::json udf_input;
  udf_input["context"] = std::move(context);
  if (const auto iter = partition.find("keyGroups"); iter == partition.end()) {
    return absl::InvalidArgumentError("Request has no keyGroups");
  } else {
    udf_input["keyGroups"] = std::move(iter.value());
  }
  udf_input["udfInputApiVersion"] = kUdfInputApiVersion;

  // Call UDF for key groups in the partition
  const auto maybe_udf_group_outputs =
      ExecuteUdfForKeyGroups(udf_client, udf_input);
  if (!maybe_udf_group_outputs.ok()) {
    return maybe_udf_group_outputs.status();
  }

  if (const auto iter =
          maybe_udf_group_outputs.value().find("udfOutputApiVersion");
      iter == maybe_udf_group_outputs.value().end()) {
    return absl::InternalError("UDF response has no udfOutputApiVersion");
  } else {
    if (iter.value() != kUdfOutputApiVersion) {
      return absl::InternalError("Invalid udfOutputApiVersion");
    }
  }

  if (const auto iter = maybe_udf_group_outputs.value().find("keyGroupOutputs");
      iter == maybe_udf_group_outputs.value().end()) {
    return absl::InternalError("UDF Response has no keyGroupOutputs");
  } else {
    if (!iter.value().is_array()) {
      return absl::InternalError("UDF keyGroupOutputs not an array");
    }
    absl::c_move(iter.value(), std::back_inserter(group_outputs));
  }
  VLOG(5) << "Generated partition output: " << partition_output;
  return partition_output;
}

// Parses the given string into a JSON object. Does not throw JSON exceptions.
absl::StatusOr<nlohmann::json> Parse(std::string_view json_string) {
  nlohmann::json core_data_json =
      nlohmann::json::parse(json_string, nullptr, /*allow_exceptions=*/false,
                            /*ignore_comments=*/true);

  if (core_data_json.is_discarded()) {
    return absl::InvalidArgumentError("Failed to parse the request json");
  }

  return core_data_json;
}

// Returns a list of JSON objects each representing a compression group, which
// is a group of partition outputs.
absl::StatusOr<std::vector<nlohmann::json>> ProcessGetValuesCoreRequest(
    const UdfClient& udf_client, const nlohmann::json& core_data_json) {
  const nlohmann::json *partitions, *context;

  // First get the partitions and context. They will be the input to the
  // processing function
  if (auto iter = core_data_json.find("partitions");
      iter == core_data_json.end()) {
    return absl::InvalidArgumentError("Request has no partitions");
  } else {
    partitions = &iter.value();
  }
  if (auto iter = core_data_json.find("context");
      iter == core_data_json.end()) {
    return absl::InvalidArgumentError("Request has no context");
  } else {
    context = &iter.value();
  }

  // For each partition, process separately and aggregate the results in
  // compression_group_map
  absl::flat_hash_map<int, nlohmann::json> compression_group_map;
  for (const auto& partition : *partitions) {
    std::int64_t compression_group = 0;
    if (auto iter = partition.find("compressionGroup");
        iter == partition.end()) {
      return absl::InvalidArgumentError(
          "compressionGroup should be set for every partition");
    } else {
      auto compression_group_ptr =
          iter.value().get_ptr<const nlohmann::json::number_integer_t*>();
      if (!compression_group_ptr) {
        return absl::InvalidArgumentError(absl::StrCat(
            "compressionGroup should be a number. Got: ", iter.value().dump()));
      }
    }

    if (auto maybe_result = ProcessPartition(udf_client, *context, partition);
        maybe_result.ok()) {
      compression_group_map[compression_group]["partitions"].emplace_back(
          std::move(maybe_result).value());
    } else {
      LOG(ERROR) << "Failed to process partition: " << maybe_result.status();
    }
  }
  std::vector<nlohmann::json> compression_groups;
  compression_groups.reserve(compression_group_map.size());
  VLOG(9) << "ProcessGetValuesCoreRequest finished successfully";
  for (auto&& [group_id, group] : compression_group_map) {
    compression_groups.push_back(std::move(group));
  }
  return compression_groups;
}

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

nlohmann::json GetValuesV2Handler::BuildCompressionGroupsForDebugging(
    std::vector<nlohmann::json> compression_groups) {
  nlohmann::json output;
  for (auto&& group : std::move(compression_groups)) {
    output.push_back(std::move(group));
  }
  return output;
}

absl::StatusOr<nlohmann::json> GetValuesV2Handler::GetValuesJsonResponse(
    const v2::GetValuesRequest& request) const {
  absl::StatusOr<nlohmann::json> maybe_core_request_json =
      Parse(request.raw_body().data());
  if (!maybe_core_request_json.ok()) {
    return maybe_core_request_json.status();
  }

  auto maybe_compression_groups =
      ProcessGetValuesCoreRequest(udf_client_, maybe_core_request_json.value());
  if (!maybe_compression_groups.ok()) {
    return maybe_compression_groups.status();
  }
  nlohmann::json response_json = BuildCompressionGroupsForDebugging(
      std::move(maybe_compression_groups).value());
  VLOG(5) << "Uncompressed response: " << response_json.dump(1);
  return response_json;
}

grpc::Status GetValuesV2Handler::GetValues(
    const GetValuesRequest& request, google::api::HttpBody* response) const {
  const auto maybe_response_json = GetValuesJsonResponse(request);
  if (maybe_response_json.ok()) {
    response->set_data(maybe_response_json.value().dump());
    return grpc::Status::OK;
  }
  return grpc::Status(StatusCode::INTERNAL,
                      std::string(maybe_response_json.status().message()));
}

grpc::Status GetValuesV2Handler::BinaryHttpGetValues(
    const BinaryHttpGetValuesRequest& bhttp_request,
    google::api::HttpBody* response) const {
  return BinaryHttpGetValues(bhttp_request.raw_body().data(),
                             *response->mutable_data());
}

absl::StatusOr<quiche::BinaryHttpResponse>
GetValuesV2Handler::BuildSuccessfulGetValuesBhttpResponse(
    std::string_view bhttp_request_body) const {
  VLOG(9) << "Handling the binary http layer";
  const absl::StatusOr<BinaryHttpRequest> maybe_deserialized_req =
      BinaryHttpRequest::Create(bhttp_request_body);
  if (!maybe_deserialized_req.ok()) {
    // Deserialization error
    VLOG(1) << "Failed to deserialize binary http request: "
            << maybe_deserialized_req.status();
    return maybe_deserialized_req.status();
  }
  VLOG(3) << "BinaryHttpGetValues request: "
          << maybe_deserialized_req->DebugString();
  absl::StatusOr<nlohmann::json> maybe_core_request_json =
      Parse(maybe_deserialized_req->body());
  if (!maybe_core_request_json.ok()) {
    return maybe_core_request_json.status();
  }

  CompressionGroupConcatenator::CompressionType compression_type =
      GetResponseCompressionType(maybe_deserialized_req->GetHeaderFields());
  std::unique_ptr<CompressionGroupConcatenator> compression_concatenator =
      create_compression_group_concatenator_(
          CompressionGroupConcatenator::CompressionType::kUncompressed);
  auto maybe_compression_groups =
      ProcessGetValuesCoreRequest(udf_client_, maybe_core_request_json.value());
  if (!maybe_compression_groups.ok()) {
    return maybe_compression_groups.status();
  }
  VLOG(9) << "Building compressed response with compression group map";
  // Compress
  for (auto&& group : std::move(maybe_compression_groups).value()) {
    compression_concatenator->AddCompressionGroup(group.dump());
  }
  absl::StatusOr<std::string> maybe_compressed_response =
      compression_concatenator->Build();
  if (!maybe_compressed_response.ok()) {
    return maybe_compressed_response.status();
  }

  VLOG(9) << "Built compressed response";
  quiche::BinaryHttpResponse bhttp_response(200);
  if (compression_type ==
      CompressionGroupConcatenator::CompressionType::kBrotli) {
    bhttp_response.AddHeaderField({std::string(kContentEncodingHeader),
                                   std::string(kBrotliAlgorithmHeader)});
  }
  // Add padding
  bhttp_response.set_body(std::move(maybe_compressed_response).value());
  return bhttp_response;
}

grpc::Status GetValuesV2Handler::BinaryHttpGetValues(
    std::string_view bhttp_request_body, std::string& response) const {
  static quiche::BinaryHttpResponse const* kDefaultBhttpResponse =
      new quiche::BinaryHttpResponse(500);
  const quiche::BinaryHttpResponse* bhttp_response = kDefaultBhttpResponse;
  absl::StatusOr<quiche::BinaryHttpResponse> maybe_successful_bhttp_response =
      BuildSuccessfulGetValuesBhttpResponse(bhttp_request_body);
  if (maybe_successful_bhttp_response.ok()) {
    bhttp_response = &(maybe_successful_bhttp_response.value());
  }
  if (auto maybe_serialized_bhttp_response = bhttp_response->Serialize();
      maybe_serialized_bhttp_response.ok()) {
    response = std::move(maybe_serialized_bhttp_response).value();
    VLOG(9) << "BinaryHttpGetValues finished successfully";
    return grpc::Status::OK;
  } else {
    return grpc::Status(
        StatusCode::INTERNAL,
        std::string(maybe_serialized_bhttp_response.status().message()));
  }
}

grpc::Status GetValuesV2Handler::ObliviousGetValues(
    const ObliviousGetValuesRequest& oblivious_request,
    google::api::HttpBody* oblivious_response) const {
  VLOG(9) << "Received ObliviousGetValues request. ";
  OhttpServerEncryptor encryptor(key_fetcher_manager_);
  auto maybe_plain_text =
      encryptor.DecryptRequest(oblivious_request.raw_body().data());
  if (!maybe_plain_text.ok()) {
    return grpc::Status(StatusCode::INTERNAL,
                        absl::StrCat(maybe_plain_text.status().code(), " : ",
                                     maybe_plain_text.status().message()));
  }
  // Now process the binary http request
  std::string response;
  if (const auto s = BinaryHttpGetValues(*maybe_plain_text, response);
      !s.ok()) {
    return s;
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

}  // namespace kv_server
