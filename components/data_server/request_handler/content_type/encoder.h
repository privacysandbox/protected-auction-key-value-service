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

#ifndef COMPONENTS_DATA_SERVER_REQUEST_HANDLER_CONTENT_TYPE_ENCODER_H_
#define COMPONENTS_DATA_SERVER_REQUEST_HANDLER_CONTENT_TYPE_ENCODER_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "components/util/request_context.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"

namespace kv_server {

// Header in clear text http request/response that indicates which format is
// used by the payload. The more common "Content-Type" header is not used
// because most importantly that has CORS implications, and in addition, may not
// be forwarded by Envoy to gRPC.
inline constexpr std::string_view kKVContentTypeHeader = "kv-content-type";

// Protobuf Content Type Header Value.
inline constexpr std::string_view kContentEncodingProtoHeaderValue =
    "message/ad-auction-trusted-signals-request+proto";
// Json Content Type Header Value.
inline constexpr std::string_view kContentEncodingJsonHeaderValue =
    "message/ad-auction-trusted-signals-request+json";
inline constexpr std::string_view kContentEncodingCborHeaderValue =
    "message/ad-auction-trusted-signals-request";

// Encodes and decodes V2 requests and responses.
class V2EncoderDecoder {
 public:
  enum class ContentType { kCbor = 0, kJson = 1, kProto = 2 };

  static ContentType GetContentType(
      const std::multimap<grpc::string_ref, grpc::string_ref>& headers,
      ContentType default_content_type);

  static std::unique_ptr<V2EncoderDecoder> Create(const ContentType& type);

  virtual ~V2EncoderDecoder() = default;

  // Encodes a V2 GetValuesResponse
  virtual absl::StatusOr<std::string> EncodeV2GetValuesResponse(
      v2::GetValuesResponse& response_proto) const = 0;

  // Encodes a list of UDF partition outputs and serializes it as a string
  virtual absl::StatusOr<std::string> EncodePartitionOutputs(
      std::vector<std::string>& partition_output_strings,
      const RequestContextFactory& request_context_factory) const = 0;

  // Decodes the string to a V2 GetValuesRequest proto
  virtual absl::StatusOr<v2::GetValuesRequest> DecodeToV2GetValuesRequestProto(
      std::string_view request) const = 0;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_REQUEST_HANDLER_CONTENT_TYPE_ENCODER_H_
