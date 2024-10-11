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

#include "components/data_server/request_handler/content_type/proto_encoder.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "nlohmann/json.hpp"
#include "public/applications/pa/api_overlay.pb.h"

namespace kv_server {

absl::StatusOr<std::string> ProtoV2EncoderDecoder::EncodeV2GetValuesResponse(
    v2::GetValuesResponse& response_proto) const {
  std::string response;
  if (!response_proto.SerializeToString(&response)) {
    auto error_message = "Cannot serialize the response as a proto.";
    return absl::InvalidArgumentError(error_message);
  }
  return response;
}

absl::StatusOr<std::string> ProtoV2EncoderDecoder::EncodePartitionOutputs(
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
    partition_output_json["id"] = partition_output_pair.first;
    json_partition_output_list.emplace_back(partition_output_json);
  }
  if (json_partition_output_list.size() == 0) {
    return absl::InvalidArgumentError(
        "No partition outputs were added to compression group content");
  }
  return json_partition_output_list.dump();
}

absl::StatusOr<v2::GetValuesRequest>
ProtoV2EncoderDecoder::DecodeToV2GetValuesRequestProto(
    std::string_view request) const {
  v2::GetValuesRequest request_proto;
  if (request.empty()) {
    return absl::InvalidArgumentError(
        "Received empty request, not converting to v2::GetValuesRequest proto");
  }
  if (!request_proto.ParseFromString(request)) {
    auto error_message = absl::StrCat(
        "Cannot parse request as a valid serialized proto object: ", request);
    return absl::InvalidArgumentError(error_message);
  }
  return request_proto;
}

}  // namespace kv_server
