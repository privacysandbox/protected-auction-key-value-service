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

#include "absl/status/statusor.h"
#include "components/telemetry/telemetry.h"
#include "glog/logging.h"
#include "grpcpp/grpcpp.h"
#include "public/base_types.pb.h"
#include "public/constants.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"
#include "quiche/binary_http/binary_http_message.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "quiche/oblivious_http/oblivious_http_gateway.h"

constexpr char* GET_VALUES_v2_HANDLER_SPAN = "GetValuesV2Handler";
constexpr char* CACHE_KEY_V2_HIT = "CacheKeyHit";
constexpr char* CACHE_KEY_V2_MISS = "CacheKeyMiss";

namespace kv_server {
namespace {
using grpc::StatusCode;
using quiche::BinaryHttpRequest;
using quiche::BinaryHttpResponse;
using v2::BinaryHttpGetValuesRequest;
using v2::GetValuesRequest;
using v2::KeyValueService;
using v2::ObliviousGetValuesRequest;

const std::string_view kOHTTPResponseContentType = "message/ohttp-res";

// Does a key value lookup for the keys in this group. This is a reference
// implementation.
absl::StatusOr<nlohmann::json> ProcessKeyGroup(
    const Cache& cache, const nlohmann::json& context,
    const nlohmann::json& key_group) {
  std::vector<std::string_view> key_list;
  for (const auto& key : key_group["keyList"]) {
    key_list.push_back(std::string_view{
        key.get_ptr<const nlohmann::json::string_t*>()->c_str()});
  }
  auto kv_pairs = cache.GetKeyValuePairs(key_list);

  nlohmann::json group_output;
  group_output["tags"] = key_group["tags"];
  auto& key_values = group_output["keyValues"];
  for (auto&& [k, v] : std::move(kv_pairs)) {
    nlohmann::json value = {{"value", v}};
    key_values.emplace(k, value);
  }
  VLOG(5) << "Generated group output: " << group_output;
  return group_output;
}
// This is a reference implementation. This would be replaced by UDF
// invocations.
absl::StatusOr<nlohmann::json> ProcessPartition(
    const Cache& cache, const nlohmann::json& context,
    const nlohmann::json& partition) {
  nlohmann::json partition_output = {{"id", partition["id"]}};
  auto& group_outputs = partition_output["keyGroupOutputs"];
  for (const auto& key_group : partition["keyGroups"]) {
    const auto& tags = key_group["tags"];
    // Structured keys are not supported
    VLOG(6) << "Processing key group with tags: " << tags;
    if (std::find(tags.begin(), tags.end(), "structured") != tags.end())
      continue;
    if (auto maybe_output = ProcessKeyGroup(cache, context, key_group);
        maybe_output.ok()) {
      group_outputs.emplace_back(std::move(maybe_output).value());
    }
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
    const Cache& cache, const nlohmann::json& core_data_json) {
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

    if (auto maybe_result = ProcessPartition(cache, *context, partition);
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

}  // namespace

nlohmann::json GetValuesV2Handler::BuildCompressionGroupsForDebugging(
    std::vector<nlohmann::json> compression_groups) {
  nlohmann::json output;
  for (auto&& group : std::move(compression_groups)) {
    output.push_back(std::move(group));
  }
  return output;
}

grpc::Status GetValuesV2Handler::GetValues(
    const GetValuesRequest& request, google::api::HttpBody* response) const {
  auto span = GetTracer()->StartSpan(GET_VALUES_v2_HANDLER_SPAN);
  auto scope = opentelemetry::trace::Scope(span);

  absl::StatusOr<nlohmann::json> maybe_core_request_json =
      Parse(request.raw_body().data());
  if (!maybe_core_request_json.ok()) {
    return grpc::Status(
        StatusCode::INTERNAL,
        std::string(maybe_core_request_json.status().message()));
  }

  if (auto maybe_compression_groups =
          ProcessGetValuesCoreRequest(cache_, maybe_core_request_json.value());
      maybe_compression_groups.ok()) {
    nlohmann::json response_json = BuildCompressionGroupsForDebugging(
        std::move(maybe_compression_groups).value());

    if (response_json.size() > 0)
      metrics_recorder_.IncrementEventCounter(CACHE_KEY_V2_HIT);
    else
      metrics_recorder_.IncrementEventCounter(CACHE_KEY_V2_MISS);

    VLOG(5) << "Uncompressed response: " << response_json.dump(1);
    response->set_data(response_json.dump());
    return grpc::Status::OK;
  } else {
    return grpc::Status(
        StatusCode::INTERNAL,
        std::string(maybe_compression_groups.status().message()));
  }
}

grpc::Status GetValuesV2Handler::BinaryHttpGetValues(
    const BinaryHttpGetValuesRequest& bhttp_request,
    google::api::HttpBody* response) const {
  return BinaryHttpGetValues(bhttp_request.raw_body().data(),
                             *response->mutable_data());
}

grpc::Status GetValuesV2Handler::BinaryHttpGetValues(
    std::string_view bhttp_request_body, std::string& response) const {
  VLOG(9) << "Handling the binary http layer";
  const absl::StatusOr<BinaryHttpRequest> maybe_deserialized_req =
      BinaryHttpRequest::Create(bhttp_request_body);
  if (!maybe_deserialized_req.ok()) {
    // Deserialization error
    VLOG(1) << "Failed to deserialize binary http request: "
            << maybe_deserialized_req.status();
    return grpc::Status(StatusCode::INTERNAL,
                        std::string(maybe_deserialized_req.status().message()));
  }
  VLOG(3) << "BinaryHttpGetValues request: "
          << maybe_deserialized_req->DebugString();
  absl::StatusOr<nlohmann::json> maybe_core_request_json =
      Parse(maybe_deserialized_req->body());
  if (!maybe_core_request_json.ok()) {
    return grpc::Status(
        StatusCode::INTERNAL,
        std::string(maybe_core_request_json.status().message()));
  }

  std::unique_ptr<CompressionGroupConcatenator> compression_concatenator =
      create_compression_group_concatenator_(
          CompressionGroupConcatenator::CompressionType::kUncompressed);
  std::string error_message;
  if (auto maybe_compression_groups =
          ProcessGetValuesCoreRequest(cache_, maybe_core_request_json.value());
      maybe_compression_groups.ok()) {
    VLOG(9) << "Building compressed response with compression group map";
    // Compress
    for (auto&& group : std::move(maybe_compression_groups).value()) {
      compression_concatenator->AddCompressionGroup(group.dump());
    }
    std::string compressed_response = compression_concatenator->Build();
    VLOG(9) << "Built compressed response";
    quiche::BinaryHttpResponse bhttp_response(200);
    // Add padding
    bhttp_response.set_body(std::move(compressed_response));
    if (auto maybe_serialized_bhttp_response = bhttp_response.Serialize();
        maybe_serialized_bhttp_response.ok()) {
      response = std::move(maybe_serialized_bhttp_response).value();
      VLOG(9) << "BinaryHttpGetValues finished successfully";
      return grpc::Status::OK;
    } else {
      error_message = maybe_serialized_bhttp_response.status().message();
    }
  } else {
    error_message = maybe_compression_groups.status().message();
  }
  VLOG(9) << "BinaryHttpGetValues failed: " << error_message;
  return grpc::Status(StatusCode::INTERNAL, error_message);
}

grpc::Status GetValuesV2Handler::ObliviousGetValues(
    const ObliviousGetValuesRequest& oblivious_request,
    google::api::HttpBody* oblivious_response) const {
  VLOG(9) << "Received ObliviousGetValues request. ";

  const absl::StatusOr<uint8_t> maybe_req_key_id = quiche::
      ObliviousHttpHeaderKeyConfig::ParseKeyIdFromObliviousHttpRequestPayload(
          oblivious_request.raw_body().data());
  if (!maybe_req_key_id.ok()) {
    return grpc::Status(StatusCode::INTERNAL,
                        absl::StrCat("Unable to get OHTTP key id: ",
                                     maybe_req_key_id.status().message()));
  }
  const auto maybe_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      *maybe_req_key_id, kKEMParameter, kKDFParameter, kAEADParameter);
  if (!maybe_config.ok()) {
    return grpc::Status(StatusCode::INTERNAL,
                        absl::StrCat("Unable to build OHTTP config: ",
                                     maybe_config.status().message()));
  }

  const auto ohttp_instance =
      quiche::ObliviousHttpGateway::Create(test_private_key_, *maybe_config);

  auto decrypted_req = ohttp_instance->DecryptObliviousHttpRequest(
      oblivious_request.raw_body().data());

  if (!decrypted_req.ok()) {
    return grpc::Status(StatusCode::INTERNAL,
                        std::string(decrypted_req.status().message()));
  }
  absl::string_view request_text = decrypted_req->GetPlaintextData();

  // Now process the binary http request
  std::string response;
  if (const auto s = BinaryHttpGetValues(request_text, response); !s.ok()) {
    return s;
  }

  // encrypt/encapsulate the response
  google::api::HttpBody bhttp_response;
  auto server_request_context =
      std::move(decrypted_req).value().ReleaseContext();
  const auto encapsulate_resp = ohttp_instance->CreateObliviousHttpResponse(
      response, server_request_context);
  if (!encapsulate_resp.ok()) {
    return grpc::Status(StatusCode::INTERNAL,
                        std::string(encapsulate_resp.status().message()));
  }
  oblivious_response->set_content_type(std::string(kOHTTPResponseContentType));
  oblivious_response->set_data(encapsulate_resp->EncapsulateAndSerialize());

  return grpc::Status::OK;
}

}  // namespace kv_server
