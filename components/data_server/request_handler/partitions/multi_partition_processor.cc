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
#include "components/udf/udf_client.h"
#include "components/util/request_context.h"
#include "public/api_schema.pb.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"

namespace kv_server {
namespace {

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
      compression_group_map[compression_group_id].emplace_back(
          partition.id(), std::move(it->second));
    } else {
      PS_VLOG(3, request_context_factory.Get().GetPSLogContext())
          << "Failed to execute UDF for partition " << partition.id()
          << " and compression_group_id " << compression_group_id;
    }
  }
  return compression_group_map;
}

}  // namespace
MultiPartitionProcessor::MultiPartitionProcessor(
    const RequestContextFactory& request_context_factory,
    const UdfClient& udf_client, const V2EncoderDecoder& v2_codec)
    : request_context_factory_(request_context_factory),
      udf_client_(udf_client),
      v2_codec_(v2_codec) {}

absl::Status MultiPartitionProcessor::Process(
    const v2::GetValuesRequest& request, v2::GetValuesResponse& response,
    ExecutionMetadata& execution_metadata) const {
  absl::flat_hash_map<UniquePartitionIdTuple, UDFInput> udf_input_map;
  for (const auto& partition : request.partitions()) {
    UDFExecutionMetadata udf_metadata;
    *udf_metadata.mutable_request_metadata() = request.metadata();
    if (!partition.metadata().fields().empty()) {
      *udf_metadata.mutable_partition_metadata() = partition.metadata();
    }
    udf_input_map.insert_or_assign(
        {partition.id(), partition.compression_group_id()},
        UDFInput{.execution_metadata = std::move(udf_metadata),
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
