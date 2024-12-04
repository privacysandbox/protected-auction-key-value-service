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

#include "components/data_server/request_handler/content_type/json_encoder.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "components/errors/error_tag.h"
#include "nlohmann/json.hpp"

namespace kv_server {

namespace {

using google::protobuf::util::MessageToJsonString;

enum class ErrorTag : int {
  kJsonEncodeV2GetValuesResponse = 1,
  kNoPartitionOutputsInCompressionGroup = 2,
  kJsonDecodeGetValuesRequest = 3,
};
}  // namespace

absl::StatusOr<std::string> JsonV2EncoderDecoder::EncodeV2GetValuesResponse(
    v2::GetValuesResponse& response_proto) const {
  std::string response;
  auto status = MessageToJsonString(response_proto, &response);
  if (!status.ok()) {
    return StatusWithErrorTag(absl::InvalidArgumentError(status.message()),
                              __FILE__,
                              ErrorTag::kJsonEncodeV2GetValuesResponse);
  }
  return response;
}

absl::StatusOr<std::string> JsonV2EncoderDecoder::EncodePartitionOutputs(
    std::vector<std::pair<int32_t, std::string>>& partition_output_pairs,
    const RequestContextFactory& request_context_factory) const {
  nlohmann::json json_partition_output_list = nlohmann::json::array();
  for (auto&& partition_output_pair : partition_output_pairs) {
    auto partition_output_json =
        nlohmann::json::parse(partition_output_pair.second, nullptr,
                              /*allow_exceptions=*/false,
                              /*ignore_comments=*/true);
    if (partition_output_json.is_discarded()) {
      PS_VLOG(2, request_context_factory.Get().GetPSLogContext())
          << "json parse failed for " << partition_output_pair.second;
      continue;
    }
    if (!partition_output_json.is_object()) {
      PS_VLOG(2, request_context_factory.Get().GetPSLogContext())
          << "json parse returned a non object" << partition_output_pair.second;
      continue;
    }
    partition_output_json["id"] = partition_output_pair.first;
    json_partition_output_list.emplace_back(partition_output_json);
  }
  if (json_partition_output_list.size() == 0) {
    return StatusWithErrorTag(
        absl::InvalidArgumentError(
            "No partition outputs were added to compression group content"),
        __FILE__, ErrorTag::kNoPartitionOutputsInCompressionGroup);
  }
  return json_partition_output_list.dump();
}

absl::StatusOr<v2::GetValuesRequest>
JsonV2EncoderDecoder::DecodeToV2GetValuesRequestProto(
    std::string_view request) const {
  v2::GetValuesRequest request_proto;
  auto status =
      google::protobuf::util::JsonStringToMessage(request, &request_proto);
  if (!status.ok()) {
    return StatusWithErrorTag(absl::InvalidArgumentError(status.message()),
                              __FILE__, ErrorTag::kJsonDecodeGetValuesRequest);
  }
  return request_proto;
}

}  // namespace kv_server
