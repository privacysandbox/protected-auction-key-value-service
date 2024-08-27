// Copyright 2024 Google LLC
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

#include "components/data/converters/cbor_converter.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "components/data/converters/cbor_converter_utils.h"
#include "components/data/converters/scoped_cbor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/json_util.h"
#include "nlohmann/json.hpp"
#include "public/applications/pa/response_utils.h"
#include "public/query/v2/get_values_v2.pb.h"
#include "src/util/status_macro/status_macros.h"

#include "cbor.h"

namespace kv_server {

namespace {
inline constexpr char kCompressionGroups[] = "compressionGroups";
inline constexpr char kCompressionGroupId[] = "compressionGroupId";
inline constexpr char kTtlMs[] = "ttlMs";
inline constexpr char kContent[] = "content";

inline constexpr char kPartitionOutputs[] = "partitionOutputs";
inline constexpr char kPartitionId[] = "id";
inline constexpr char kKeyGroupOutputs[] = "keyGroupOutputs";
inline constexpr char kTags[] = "tags";
inline constexpr char kKeyValues[] = "keyValues";
inline constexpr char kValue[] = "value";

absl::StatusOr<cbor_item_t*> EncodeCompressionGroup(
    v2::CompressionGroup& compression_group) {
  const int compressionGroupKeysNumber = 3;
  auto* cbor_internal = cbor_new_definite_map(compressionGroupKeysNumber);
  if (compression_group.has_ttl_ms()) {
    PS_RETURN_IF_ERROR(
        CborSerializeUInt(kTtlMs, compression_group.ttl_ms(), *cbor_internal));
  }
  PS_RETURN_IF_ERROR(CborSerializeByteString(
      kContent, std::move(compression_group.content()), *cbor_internal));
  PS_RETURN_IF_ERROR(CborSerializeUInt(kCompressionGroupId,
                                       compression_group.compression_group_id(),
                                       *cbor_internal));

  return cbor_internal;
}

absl::StatusOr<cbor_item_t*> EncodeCompressionGroups(
    google::protobuf::RepeatedPtrField<v2::CompressionGroup>&
        compression_groups) {
  cbor_item_t* serialized_compression_groups =
      cbor_new_definite_array(compression_groups.size());
  for (auto& compression_group : compression_groups) {
    PS_ASSIGN_OR_RETURN(auto* serialized_compression_group,
                        EncodeCompressionGroup(compression_group));
    if (!cbor_array_push(serialized_compression_groups,
                         cbor_move(serialized_compression_group))) {
      return absl::InternalError(absl::StrCat("Failed to serialize ",
                                              kCompressionGroups, " to CBOR. ",
                                              compression_group));
    }
  }

  return serialized_compression_groups;
}

absl::StatusOr<cbor_item_t*> EncodeKeyGroupOutput(
    application_pa::KeyGroupOutput& key_group_output) {
  const int keyGroupOutputKeysNumber = 2;
  auto* cbor_internal = cbor_new_definite_map(keyGroupOutputKeysNumber);
  // tags
  cbor_item_t* serialized_tags =
      cbor_new_definite_array(key_group_output.tags().size());

  for (auto& tag : key_group_output.tags()) {
    if (!cbor_array_push(serialized_tags, cbor_move(cbor_build_stringn(
                                              tag.data(), tag.size())))) {
      return absl::InternalError(absl::StrCat("Failed to serialize ", kTags,
                                              " to CBOR. ", key_group_output));
    }
  }
  struct cbor_pair serialized_serialized_tags_pair = {
      .key = cbor_move(cbor_build_stringn(kTags, sizeof(kTags) - 1)),
      .value = serialized_tags,
  };
  if (!cbor_map_add(cbor_internal, serialized_serialized_tags_pair)) {
    return absl::InternalError(absl::StrCat("Failed to serialize ", kTags,
                                            " to CBOR. ", key_group_output));
  }
  // key_values
  cbor_item_t* serialized_key_values =
      cbor_new_definite_map(key_group_output.key_values().size());
  std::vector<std::pair<std::string, cbor_pair>> kv_vector;
  for (auto&& [key, value] : *(key_group_output.mutable_key_values())) {
    std::string value_str = std::move(value.mutable_value()->string_value());
    auto* cbor_internal_value = cbor_new_definite_map(1);
    struct cbor_pair serialized_value_pair = {
        .key = cbor_move(cbor_build_stringn(kValue, sizeof(kValue) - 1)),
        .value =
            cbor_move(cbor_build_stringn(value_str.c_str(), value_str.size())),
    };

    if (!cbor_map_add(cbor_internal_value, serialized_value_pair)) {
      return absl::InternalError(absl::StrCat("Failed to serialize ", kValue,
                                              " to CBOR. ", key_group_output));
    }
    struct cbor_pair serialized_key_value_pair = {
        .key = cbor_move(cbor_build_stringn(key.c_str(), key.size())),
        .value = cbor_internal_value,
    };
    kv_vector.emplace_back(key, serialized_key_value_pair);
  }
  // Following the chromium implementation, we only need to check that
  // the length and lexicographic order of the plaintext string
  // https://chromium.googlesource.com/chromium/src/components/cbor/+/10d0a11b998d2cca774189ba26159ad4e1eacb7f/values.h#59
  // https://chromium.googlesource.com/chromium/src/components/cbor/+/10d0a11b998d2cca774189ba26159ad4e1eacb7f/values.cc#109
  std::sort(kv_vector.begin(), kv_vector.end(), [](auto& left, auto& right) {
    const auto left_size = left.first.size();
    const auto& left_str = left.first;
    const auto right_size = right.first.size();
    const auto& right_str = right.first;
    return std::tie(left_size, left_str) < std::tie(right_size, right_str);
  });
  for (auto&& [key, serialized_key_value_pair] : kv_vector) {
    if (!cbor_map_add(serialized_key_values, serialized_key_value_pair)) {
      return absl::InternalError(absl::StrCat(
          "Failed to serialize ", kKeyValues, " to CBOR. ", key_group_output));
    }
  }
  struct cbor_pair serialized_key_values_pair = {
      .key = cbor_move(cbor_build_stringn(kKeyValues, sizeof(kKeyValues) - 1)),
      .value = serialized_key_values,
  };
  if (!cbor_map_add(cbor_internal, serialized_key_values_pair)) {
    return absl::InternalError(absl::StrCat("Failed to serialize ", kKeyValues,
                                            " to CBOR. ", key_group_output));
  }
  return cbor_internal;
}

absl::StatusOr<cbor_item_t*> EncodePartitionOutput(
    application_pa::PartitionOutput& partition_output) {
  const int partitionKeysNumber = 2;
  auto* cbor_internal = cbor_new_definite_map(partitionKeysNumber);
  PS_RETURN_IF_ERROR(
      CborSerializeUInt(kPartitionId, partition_output.id(), *cbor_internal));
  cbor_item_t* serialized_key_group_outputs =
      cbor_new_definite_array(partition_output.key_group_outputs().size());
  for (auto& key_group_output :
       *(partition_output.mutable_key_group_outputs())) {
    PS_ASSIGN_OR_RETURN(auto* serialized_key_group_output,
                        EncodeKeyGroupOutput(key_group_output));
    if (!cbor_array_push(serialized_key_group_outputs,
                         cbor_move(serialized_key_group_output))) {
      return absl::InternalError(absl::StrCat("Failed to serialize ",
                                              kPartitionOutputs, " to CBOR",
                                              partition_output));
    }
  }
  struct cbor_pair serialized_key_group_outputs_pair = {
      .key = cbor_move(
          cbor_build_stringn(kKeyGroupOutputs, sizeof(kKeyGroupOutputs) - 1)),
      .value = serialized_key_group_outputs,
  };
  if (!cbor_map_add(cbor_internal, serialized_key_group_outputs_pair)) {
    return absl::InternalError(absl::StrCat("Failed to serialize ",
                                            kKeyGroupOutputs, " to CBOR. ",
                                            partition_output));
  }
  return cbor_internal;
}

absl::Status EncodePartitionOutputs(
    google::protobuf::RepeatedPtrField<application_pa::PartitionOutput>&
        partition_outputs,
    cbor_item_t* serialized_partition_outputs) {
  for (auto& partition_output : partition_outputs) {
    PS_ASSIGN_OR_RETURN(auto* serialized_partition_output,
                        EncodePartitionOutput(partition_output));
    if (!cbor_array_push(serialized_partition_outputs,
                         cbor_move(serialized_partition_output))) {
      return absl::InternalError(absl::StrCat("Failed to serialize ",
                                              kPartitionOutputs, " to CBOR. ",
                                              partition_output));
    }
  }
  return absl::OkStatus();
}

}  // namespace
absl::StatusOr<std::string> V2GetValuesResponseCborEncode(
    v2::GetValuesResponse& response) {
  if (response.has_single_partition()) {
    return absl::InvalidArgumentError(
        "single_partition is not supported for cbor content type");
  }
  const int getValuesResponseKeysNumber = 1;
  ScopedCbor root(cbor_new_definite_map(getValuesResponseKeysNumber));
  PS_ASSIGN_OR_RETURN(
      auto* compression_groups,
      EncodeCompressionGroups(*(response.mutable_compression_groups())));
  struct cbor_pair serialized_compression_groups = {
      .key = cbor_move(cbor_build_stringn(kCompressionGroups,
                                          sizeof(kCompressionGroups) - 1)),
      .value = compression_groups,
  };
  auto* cbor_internal = root.get();
  if (!cbor_map_add(cbor_internal, serialized_compression_groups)) {
    return absl::InternalError(absl::StrCat(
        "Failed to serialize ", kCompressionGroups, " to CBOR. ", response));
  }
  return GetCborSerializedResult(*cbor_internal);
}

absl::StatusOr<std::string> V2GetValuesRequestJsonStringCborEncode(
    std::string_view serialized_json) {
  nlohmann::json json_req = nlohmann::json::parse(serialized_json, nullptr,
                                                  /*allow_exceptions=*/false,
                                                  /*ignore_comments=*/true);
  if (json_req.is_discarded()) {
    return absl::InternalError(absl::StrCat(
        "Unable to parse json req from string: ", serialized_json));
  }
  std::vector<uint8_t> cbor_vec = nlohmann::json::to_cbor(json_req);
  return std::string(cbor_vec.begin(), cbor_vec.end());
}

absl::StatusOr<std::string> V2GetValuesRequestProtoToCborEncode(
    const v2::GetValuesRequest& proto_req) {
  std::string json_req_string;
  if (const auto json_status = google::protobuf::json::MessageToJsonString(
          proto_req, &json_req_string);
      !json_status.ok()) {
    return absl::InternalError(absl::StrCat(
        "Unable to convert proto request to json string: ", proto_req));
  }
  return V2GetValuesRequestJsonStringCborEncode(json_req_string);
}

absl::StatusOr<std::string> PartitionOutputsCborEncode(
    google::protobuf::RepeatedPtrField<application_pa::PartitionOutput>&
        partition_outputs) {
  ScopedCbor root(cbor_new_definite_array(partition_outputs.size()));
  auto* cbor_internal = root.get();
  PS_RETURN_IF_ERROR(EncodePartitionOutputs(partition_outputs, cbor_internal));
  return GetCborSerializedResult(*cbor_internal);
}

absl::StatusOr<nlohmann::json> GetPartitionOutputsInJson(
    const nlohmann::json& content_json) {
  std::vector<uint8_t> content_cbor = nlohmann::json::to_cbor(content_json);
  std::string content_cbor_string =
      std::string(content_cbor.begin(), content_cbor.end());
  struct cbor_load_result result;
  cbor_item_t* cbor_bytestring = cbor_load(
      reinterpret_cast<const unsigned char*>(content_cbor_string.data()),
      content_cbor_string.size(), &result);
  auto partition_output_cbor = cbor_bytestring_handle(cbor_bytestring);
  auto cbor_bytestring_len = cbor_bytestring_length(cbor_bytestring);
  return nlohmann::json::from_cbor(std::vector<uint8_t>(
      partition_output_cbor, partition_output_cbor + cbor_bytestring_len));
}

absl::StatusOr<std::string> V2CompressionGroupCborEncode(
    application_pa::V2CompressionGroup& comp_group) {
  const int getCompressionGroupKeysNumber = 1;
  ScopedCbor root(cbor_new_definite_map(getCompressionGroupKeysNumber));
  cbor_item_t* partition_outputs =
      cbor_new_definite_array(comp_group.partition_outputs().size());
  PS_RETURN_IF_ERROR(EncodePartitionOutputs(
      *(comp_group.mutable_partition_outputs()), partition_outputs));
  struct cbor_pair serialized_partition_outputs = {
      .key = cbor_move(
          cbor_build_stringn(kPartitionOutputs, sizeof(kPartitionOutputs) - 1)),
      .value = partition_outputs,
  };
  auto* cbor_internal = root.get();
  if (!cbor_map_add(cbor_internal, serialized_partition_outputs)) {
    return absl::InternalError(absl::StrCat(
        "Failed to serialize ", kPartitionOutputs, " to CBOR. ", comp_group));
  }
  return GetCborSerializedResult(*cbor_internal);
}

absl::Status CborDecodeToNonBytesProto(std::string_view cbor_raw,
                                       google::protobuf::Message& message) {
  // TODO(b/353537363): Skip intermediate JSON conversion step
  nlohmann::json json_from_cbor = nlohmann::json::from_cbor(
      cbor_raw, /*strict=*/true, /*allow_exceptions=*/false);
  if (json_from_cbor.is_discarded()) {
    return absl::InternalError("Failed to convert raw CBOR buffer to JSON");
  }
  return google::protobuf::util::JsonStringToMessage(json_from_cbor.dump(),
                                                     &message);
}

}  // namespace kv_server
