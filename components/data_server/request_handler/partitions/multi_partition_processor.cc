// Copyright 2025 Google LLC
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

#include "components/data_server/request_handler/partitions/multi_partition_processor.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "components/data_server/request_handler/content_type/encoder.h"
#include "components/data_server/request_handler/status/status_tag.h"
#include "components/errors/error_tag.h"
#include "components/udf/udf_client.h"
#include "components/util/request_context.h"
#include "public/api_schema.pb.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"

namespace kv_server {
namespace {

enum class ErrorTag : int {
  kDuplicatePerPartitionMetadataForAllPartitions = 1,
  kNoValueForPerPartitionMetadata = 2,
  kNotAListError = 3,
  kNoStringValueForPerPartitionMetadata = 4,
  kDuplicateMetadataInPartitionMetadataAndPerPartitionMetadata = 5,
  kIdsNotTwoNumbers = 6,
  kNotAListOrUnsetError = 7,
};

constexpr std::string_view kValue = "value";
constexpr std::string_view kIds = "ids";

using google::protobuf::Map;
using google::protobuf::Struct;
using google::protobuf::Value;

bool HasDuplicatePartitionAndCompressionGroupIds(
    const v2::GetValuesRequest& request) {
  absl::flat_hash_set<UniquePartitionIdTuple> unique_ids;
  for (auto&& partition : request.partitions()) {
    if (unique_ids.contains(
            {partition.id(), partition.compression_group_id()})) {
      return true;
    }
    unique_ids.insert({partition.id(), partition.compression_group_id()});
  }
  return false;
}

absl::flat_hash_map<int32_t, std::vector<std::pair<int32_t, std::string>>>
BuildCompressionGroupToPartitionOutputMap(
    const v2::GetValuesRequest& request,
    const absl::flat_hash_map<UniquePartitionIdTuple, std::string>&
        id_to_output_map,
    const RequestContextFactory& request_context_factory) {
  absl::flat_hash_map<int32_t, std::vector<std::pair<int32_t, std::string>>>
      compression_group_map;
  for (const auto& partition : request.partitions()) {
    int32_t compression_group_id = partition.compression_group_id();
    auto it = id_to_output_map.find({partition.id(), compression_group_id});
    if (it != id_to_output_map.end()) {
      PS_VLOG(9, request_context_factory.Get().GetPSLogContext())
          << "UDF output for partition.id " << partition.id()
          << " and compression_group_id " << compression_group_id << ": "
          << it->second;
      compression_group_map[compression_group_id].emplace_back(
          partition.id(), std::move(it->second));
    } else {
      PS_VLOG(3, request_context_factory.Get().GetPSLogContext())
          << "Failed to execute UDF for partition.id " << partition.id()
          << " and compression_group_id " << compression_group_id;
    }
  }
  return compression_group_map;
}

absl::Status CheckValueIsListOrNotSet(std::string_view metadata_name,
                                      const Value& field_value) {
  if (field_value.has_list_value() ||
      field_value.kind_case() == Value::KIND_NOT_SET) {
    return absl::OkStatus();
  }

  return StatusWithErrorTag(
      absl::InvalidArgumentError(
          absl::StrCat("Each per_partition_metadata <k,v> entry requires v "
                       "to be a list of value configurations as defined in "
                       "public/get_values_v2.proto. Instead found k: ",
                       metadata_name, " v: ", field_value)),
      __FILE__, ErrorTag::kNotAListOrUnsetError);
}

absl::Status CheckValueIsList(std::string_view metadata_name,
                              const Value& field_value) {
  if (field_value.has_list_value()) {
    return absl::OkStatus();
  }

  return StatusWithErrorTag(
      absl::InvalidArgumentError(
          absl::StrCat("Each per_partition_metadata <k,v> entry requires v "
                       "to be a list of value configurations as defined in "
                       "public/get_values_v2.proto. Instead found k: ",
                       metadata_name, " v: ", field_value)),
      __FILE__, ErrorTag::kNotAListError);
}

absl::StatusOr<Value> GetMetadataValue(
    const Map<std::string, Value>& value_config_fields,
    std::string_view metadata_name) {
  auto value_it = value_config_fields.find(std::string(kValue));
  if (value_it == value_config_fields.end()) {
    return StatusWithErrorTag(
        absl::InvalidArgumentError(absl::StrCat(
            "No \"value\" field found for entry in per_partition_metadata ",
            metadata_name,
            ". For expected format see public/get_values_v2.proto")),
        __FILE__, ErrorTag::kNoValueForPerPartitionMetadata);
  }
  if (!value_it->second.has_string_value()) {
    return StatusWithErrorTag(
        absl::InvalidArgumentError(absl::StrCat(
            "\"value\" is not a string value for key ", metadata_name,
            ". For expected format see public/get_values_v2.proto")),
        __FILE__, ErrorTag::kNoStringValueForPerPartitionMetadata);
  }
  return value_it->second;
}

// Try inserting into the struct.
// Return error if there is a there is already an existing key with
// metadata_name.
absl::Status TryInsert(Struct& metadata, std::string_view metadata_name,
                       Value metadata_value) {
  if (metadata.fields().contains(metadata_name)) {
    return StatusWithErrorTag(
        absl::InvalidArgumentError(
            absl::StrCat("Duplicate entries found for key in "
                         "request.per_partition_metadata: ",
                         metadata_name)),
        __FILE__, ErrorTag::kDuplicatePerPartitionMetadataForAllPartitions);
  }
  (*metadata.mutable_fields())[std::string(metadata_name)] =
      std::move(metadata_value);
  return absl::OkStatus();
}

// Try inserting into the map.
// Return error if there is already an existing key with
// metadata_name.
absl::Status TryInsert(absl::flat_hash_map<UniquePartitionIdTuple, Struct>&
                           id_to_per_partition_metadata,
                       const std::vector<UniquePartitionIdTuple>& ids,
                       std::string_view metadata_name,
                       const Value& metadata_value) {
  for (auto& id : ids) {
    auto& metadata = id_to_per_partition_metadata[id];
    RETURN_IF_ERROR(TryInsert(metadata, metadata_name, metadata_value));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<UniquePartitionIdTuple>> GetUniquePartitionIds(
    std::string_view metadata_name, const Value& ids_value) {
  RETURN_IF_ERROR(CheckValueIsList(metadata_name, ids_value));
  std::vector<UniquePartitionIdTuple> unique_ids;
  for (auto& ids : ids_value.list_value().values()) {
    RETURN_IF_ERROR(CheckValueIsList(metadata_name, ids));
    if (ids.list_value().values().size() != 2 ||
        !ids.list_value().values(0).has_number_value() ||
        !ids.list_value().values(1).has_number_value()) {
      return StatusWithErrorTag(
          absl::InvalidArgumentError(absl::StrCat(
              "per_partition_metadata requires ids to be a list of "
              "2 numbers, the compression group id and partition "
              "id, e.g. [1, 0]. Found: ",
              ids.list_value())),
          __FILE__, ErrorTag::kIdsNotTwoNumbers);
    }
    auto compression_group_id = ids.list_value().values(0).number_value();
    auto partition_id = ids.list_value().values(1).number_value();
    unique_ids.emplace_back(partition_id, compression_group_id);
  }
  return unique_ids;
}

absl::Status CombinePartitionMetadata(
    absl::flat_hash_map<UniquePartitionIdTuple, UDFExecutionMetadata>&
        id_to_udf_metadata_map,
    const Struct& metadata_for_all_partitions,
    const absl::flat_hash_map<UniquePartitionIdTuple, Struct>&
        id_to_per_partition_metadata) {
  if (metadata_for_all_partitions.fields().empty()) {
    return absl::OkStatus();
  }
  auto per_partition_metadata_for_all_partitions_size =
      metadata_for_all_partitions.fields().size();
  for (auto& [id, udf_metadata] : id_to_udf_metadata_map) {
    Struct* partition_metadata = udf_metadata.mutable_partition_metadata();
    auto old_partition_metadata_size = partition_metadata->fields().size();

    // Merge per partition metadata that applies to all partitions
    partition_metadata->MergeFrom(metadata_for_all_partitions);

    // Merge per partition metadata is it exists for this id
    size_t per_partition_metadata_size = 0;
    auto it = id_to_per_partition_metadata.find(id);
    if (it != id_to_per_partition_metadata.end()) {
      per_partition_metadata_size = it->second.fields().size();
      partition_metadata->MergeFrom(it->second);
    }

    // Check if the sizes add up. If not, there were duplicates.
    auto final_partition_metadata_size = partition_metadata->fields().size();
    if (old_partition_metadata_size +
            per_partition_metadata_for_all_partitions_size +
            per_partition_metadata_size !=
        final_partition_metadata_size) {
      return StatusWithErrorTag(
          absl::InvalidArgumentError(
              "Duplicate metadata defined in request.partition[*].metadata and "
              "request.per_partition_metadata."),
          __FILE__,
          ErrorTag::
              kDuplicateMetadataInPartitionMetadataAndPerPartitionMetadata);
    }
  }
  return absl::OkStatus();
}

absl::Status ProcessPerPartitionMetadata(
    const v2::GetValuesRequest& request,
    const RequestContextFactory& request_context_factory,
    absl::flat_hash_map<UniquePartitionIdTuple, UDFExecutionMetadata>&
        id_to_udf_metadata_map) {
  PS_VLOG(9, request_context_factory.Get().GetPSLogContext())
      << "Processing per_partition_metadata";
  absl::flat_hash_map<UniquePartitionIdTuple, Struct>
      id_to_per_partition_metadata;
  Struct per_partition_metadata_for_all_partitions;

  for (auto&& [metadata_name, field_value] :
       request.per_partition_metadata().fields()) {
    RETURN_IF_ERROR(CheckValueIsListOrNotSet(metadata_name, field_value));
    // Iterate through each value for metadata name
    for (auto&& value_config_struct : field_value.list_value().values()) {
      const auto& value_config_fields =
          value_config_struct.struct_value().fields();
      PS_ASSIGN_OR_RETURN(auto metadata_value,
                          GetMetadataValue(value_config_fields, metadata_name));
      auto ids_it = value_config_fields.find(kIds);
      if (ids_it == value_config_fields.end()) {
        // No "ids" field indicates that this value should apply to all
        // partitions. Add it to `per_partition_metadata_for_all_partitions` and
        // add the "global" metadata to all partitions at the end for
        // efficiency.
        RETURN_IF_ERROR(TryInsert(per_partition_metadata_for_all_partitions,
                                  metadata_name, std::move(metadata_value)));
      } else {
        // If there is an "ids" field, then we need to add the metadata pair
        // to the listed partitions
        PS_ASSIGN_OR_RETURN(
            auto ids, GetUniquePartitionIds(metadata_name, ids_it->second));
        RETURN_IF_ERROR(TryInsert(id_to_per_partition_metadata, ids,
                                  metadata_name, metadata_value));
      }
    }
  }
  PS_VLOG(7, request_context_factory.Get().GetPSLogContext())
      << "Built per_partition_metadata_for_all_partitions: "
      << per_partition_metadata_for_all_partitions;
  RETURN_IF_ERROR(CombinePartitionMetadata(
      id_to_udf_metadata_map, per_partition_metadata_for_all_partitions,
      id_to_per_partition_metadata));
  return absl::OkStatus();
}

absl::StatusOr<
    absl::flat_hash_map<UniquePartitionIdTuple, UDFExecutionMetadata>>
BuildUdfMetadataMap(const v2::GetValuesRequest& request,
                    const RequestContextFactory& request_context_factory,
                    bool enable_per_partition_metadata) {
  absl::flat_hash_map<UniquePartitionIdTuple, UDFExecutionMetadata>
      id_to_udf_metadata_map;
  for (const auto& partition : request.partitions()) {
    UniquePartitionIdTuple id{partition.id(), partition.compression_group_id()};
    UDFExecutionMetadata udf_metadata;
    if (request.has_metadata()) {
      *udf_metadata.mutable_request_metadata() = request.metadata();
    }
    if (!partition.metadata().fields().empty()) {
      *udf_metadata.mutable_partition_metadata() = partition.metadata();
    }
    id_to_udf_metadata_map.insert_or_assign(std::move(id),
                                            std::move(udf_metadata));
  }
  if (enable_per_partition_metadata) {
    PS_RETURN_IF_ERROR(ProcessPerPartitionMetadata(
        request, request_context_factory, id_to_udf_metadata_map));
  }
  return id_to_udf_metadata_map;
}

}  // namespace

MultiPartitionProcessor::MultiPartitionProcessor(
    const RequestContextFactory& request_context_factory,
    const UdfClient& udf_client, const V2EncoderDecoder& v2_codec,
    bool enable_per_partition_metadata)
    : request_context_factory_(request_context_factory),
      udf_client_(udf_client),
      v2_codec_(v2_codec),
      enable_per_partition_metadata_(enable_per_partition_metadata) {}

absl::Status MultiPartitionProcessor::Process(
    const v2::GetValuesRequest& request, v2::GetValuesResponse& response,
    ExecutionMetadata& execution_metadata) const {
  if (HasDuplicatePartitionAndCompressionGroupIds(request)) {
    return V2RequestFormatErrorAsExternalHttpError(absl::InvalidArgumentError(
        "Each partition must have a unique <id, "
        "compression_group_id> tuple, but duplicates were found."));
  }

  absl::flat_hash_map<UniquePartitionIdTuple, UDFInput> udf_input_map;
  auto id_to_udf_metadata_map = BuildUdfMetadataMap(
      request, request_context_factory_, enable_per_partition_metadata_);
  if (!id_to_udf_metadata_map.ok()) {
    PS_VLOG(1, request_context_factory_.Get().GetPSLogContext())
        << "Error building partition metadata map";
    return V2RequestFormatErrorAsExternalHttpError(
        std::move(id_to_udf_metadata_map.status()));
  }
  for (const auto& partition : request.partitions()) {
    UniquePartitionIdTuple id{partition.id(), partition.compression_group_id()};
    UDFExecutionMetadata udf_metadata;
    auto it = id_to_udf_metadata_map->find(id);
    if (it != id_to_udf_metadata_map->end()) {
      udf_metadata = std::move(it->second);
    }
    udf_input_map.insert_or_assign(
        id, UDFInput{.execution_metadata = std::move(udf_metadata),
                     .arguments = partition.arguments()});
  }

  PS_ASSIGN_OR_RETURN(
      auto id_to_output_map,
      udf_client_.BatchExecuteCode(request_context_factory_, udf_input_map,
                                   execution_metadata));

  // Build a map of compression_group_id to <partition_id, udf_output>
  absl::flat_hash_map<int32_t, std::vector<std::pair<int32_t, std::string>>>
      compression_group_map = BuildCompressionGroupToPartitionOutputMap(
          request, id_to_output_map, request_context_factory_);

  // The content of each compressed blob is a CBOR/JSON list of partition
  // outputs or a V2CompressionGroup protobuf message.
  for (auto& [group_id, partition_output_pairs] : compression_group_map) {
    const auto maybe_content = v2_codec_.EncodePartitionOutputs(
        partition_output_pairs, request_context_factory_);
    if (!maybe_content.ok()) {
      PS_VLOG(3, request_context_factory_.Get().GetPSLogContext())
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

}  // namespace kv_server
