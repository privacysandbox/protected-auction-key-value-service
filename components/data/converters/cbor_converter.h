/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_DATA_CONVERTER_H
#define COMPONENTS_DATA_CONVERTER_H

#include <string>

#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"
#include "public/applications/pa/api_overlay.pb.h"
#include "public/query/v2/get_values_v2.pb.h"

namespace kv_server {

absl::StatusOr<std::string> V2GetValuesResponseCborEncode(
    v2::GetValuesResponse& response);

absl::StatusOr<std::string> V2CompressionGroupCborEncode(
    application_pa::V2CompressionGroup& comp_group);

absl::StatusOr<std::string> V2GetValuesRequestJsonStringCborEncode(
    std::string_view serialized_json);

absl::StatusOr<std::string> V2GetValuesRequestProtoToCborEncode(
    const v2::GetValuesRequest& proto_req);

absl::StatusOr<std::string> PartitionOutputsCborEncode(
    google::protobuf::RepeatedPtrField<application_pa::PartitionOutput>&
        partition_outputs);

absl::StatusOr<nlohmann::json> GetPartitionOutputsInJson(
    const nlohmann::json& content_json);

// Converts a CBOR serialized string to a proto that does not contain a `bytes`
// field. Will return error if the proto contains `bytes`.
absl::Status CborDecodeToNonBytesProto(std::string_view cbor_raw,
                                       google::protobuf::Message& message);
}  // namespace kv_server
#endif  // COMPONENTS_DATA_CONVERTER_H
