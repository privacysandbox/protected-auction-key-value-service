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

#include "components/data_server/request_handler/content_type/cbor_encoder.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "components/data/converters/cbor_converter.h"
#include "public/applications/pa/response_utils.h"
#include "src/util/status_macro/status_macros.h"

namespace kv_server {

absl::StatusOr<std::string> CborV2EncoderDecoder::EncodeV2GetValuesResponse(
    v2::GetValuesResponse& response_proto) const {
  PS_ASSIGN_OR_RETURN(std::string response,
                      V2GetValuesResponseCborEncode(response_proto));
  return response;
}

absl::StatusOr<std::string> CborV2EncoderDecoder::EncodePartitionOutputs(
    std::vector<std::pair<int32_t, std::string>>& partition_output_pairs,
    const RequestContextFactory& request_context_factory) const {
  google::protobuf::RepeatedPtrField<application_pa::PartitionOutput>
      partition_outputs;
  for (auto& partition_output_pair : partition_output_pairs) {
    auto partition_output =
        application_pa::PartitionOutputFromJson(partition_output_pair.second);
    if (partition_output.ok()) {
      partition_output.value().set_id(partition_output_pair.first);
      *partition_outputs.Add() = partition_output.value();
    } else {
      PS_VLOG(2, request_context_factory.Get().GetPSLogContext())
          << partition_output.status();
    }
  }

  if (partition_outputs.empty()) {
    return absl::InternalError(
        "Parsing partition output proto from json failed for all outputs");
  }

  const auto cbor_string = PartitionOutputsCborEncode(partition_outputs);
  if (!cbor_string.ok()) {
    PS_VLOG(2, request_context_factory.Get().GetPSLogContext())
        << "CBOR encode failed for partition outputs";
    return cbor_string.status();
  }
  return cbor_string.value();
}

absl::StatusOr<v2::GetValuesRequest>
CborV2EncoderDecoder::DecodeToV2GetValuesRequestProto(
    std::string_view request) const {
  v2::GetValuesRequest request_proto;
  PS_RETURN_IF_ERROR(CborDecodeToNonBytesProto(request, request_proto));
  return request_proto;
}

}  // namespace kv_server
