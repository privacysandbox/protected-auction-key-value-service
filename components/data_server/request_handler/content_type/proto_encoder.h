/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string>
#include <vector>

#include "components/data_server/request_handler/content_type/encoder.h"

namespace kv_server {

// Handles proto encoding/decoding for V2 API requests/responses
class ProtoV2EncoderDecoder : public V2EncoderDecoder {
 public:
  ProtoV2EncoderDecoder() = default;

  absl::StatusOr<std::string> EncodeV2GetValuesResponse(
      v2::GetValuesResponse& response_proto) const override;

  // Returns a serialized JSON array of partition outputs.
  // A partition output is simply the return value of a UDF execution.
  absl::StatusOr<std::string> EncodePartitionOutputs(
      std::vector<std::string>& partition_output_strings,
      const RequestContextFactory& request_context_factory) const override;

  absl::StatusOr<v2::GetValuesRequest> DecodeToV2GetValuesRequestProto(
      std::string_view request) const override;
};

}  // namespace kv_server
