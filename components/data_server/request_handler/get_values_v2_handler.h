/*
 * Copyright 2022 Google LLC
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

#ifndef COMPONENTS_DATA_SERVER_REQUEST_HANDLER_GET_VALUES_V2_HANDLER_H_
#define COMPONENTS_DATA_SERVER_REQUEST_HANDLER_GET_VALUES_V2_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/request_handler/compression.h"
#include "components/telemetry/server_definition.h"
#include "components/udf/udf_client.h"
#include "components/util/request_context.h"
#include "grpcpp/grpcpp.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"
#include "quiche/binary_http/binary_http_message.h"
#include "src/encryption/key_fetcher/key_fetcher_manager.h"

namespace kv_server {

// Content Type Header Name. Can be set for bhttp request to proto or json
// values below.
inline constexpr std::string_view kContentTypeHeader = "content-type";
// Header in clear text http request/response that indicates which format is
// used by the payload. The more common "Content-Type" header is not used
// because most importantly that has CORS implications, and in addition, may not
// be forwarded by Envoy to gRPC.
inline constexpr std::string_view kKVContentTypeHeader = "kv-content-type";
// Protobuf Content Type Header Value.
inline constexpr std::string_view kContentEncodingProtoHeaderValue =
    "application/protobuf";
// Json Content Type Header Value.
inline constexpr std::string_view kContentEncodingJsonHeaderValue =
    "application/json";
inline constexpr std::string_view kContentEncodingBhttpHeaderValue =
    "message/bhttp";

// Handles the request family of *GetValues.
// See the Service proto definition for details.
class GetValuesV2Handler {
 public:
  // Accepts a functor to create compression blob builder for testing purposes.
  explicit GetValuesV2Handler(
      const UdfClient& udf_client,
      privacy_sandbox::server_common::KeyFetcherManagerInterface&
          key_fetcher_manager,
      std::function<CompressionGroupConcatenator::FactoryFunctionType>
          create_compression_group_concatenator =
              &CompressionGroupConcatenator::Create)
      : udf_client_(udf_client),
        create_compression_group_concatenator_(
            std::move(create_compression_group_concatenator)),
        key_fetcher_manager_(key_fetcher_manager) {}

  grpc::Status GetValuesHttp(
      RequestContextFactory& request_context_factory,
      const std::multimap<grpc::string_ref, grpc::string_ref>& headers,
      const v2::GetValuesHttpRequest& request, google::api::HttpBody* response,
      ExecutionMetadata& execution_metadata) const;

  grpc::Status GetValues(RequestContextFactory& request_context_factory,
                         const v2::GetValuesRequest& request,
                         v2::GetValuesResponse* response,
                         ExecutionMetadata& execution_metadata) const;

  grpc::Status BinaryHttpGetValues(
      RequestContextFactory& request_context_factory,
      const v2::BinaryHttpGetValuesRequest& request,
      google::api::HttpBody* response,
      ExecutionMetadata& execution_metadata) const;

  // Supports requests encrypted with a fixed key for debugging/demoing.
  // X25519 Secret key (priv key).
  // https://www.ietf.org/archive/id/draft-ietf-ohai-ohttp-03.html#appendix-A-2
  // 3c168975674b2fa8e465970b79c8dcf09f1c741626480bd4c6162fc5b6a98e1a
  //
  // The corresponding public key is
  // 31e1f05a740102115220e9af918f738674aec95f54db6e04eb705aae8e798155
  //
  // HPKE Configuration must be:
  // KEM: DHKEM(X25519, HKDF-SHA256) 0x0020
  // KDF: HKDF-SHA256 0x0001
  // AEAD: AES-256-GCM 0X0002
  // (https://github.com/WICG/turtledove/blob/main/FLEDGE_Key_Value_Server_API.md#encryption)
  grpc::Status ObliviousGetValues(
      RequestContextFactory& request_context_factory,
      const std::multimap<grpc::string_ref, grpc::string_ref>& headers,
      const v2::ObliviousGetValuesRequest& request,
      google::api::HttpBody* response,
      ExecutionMetadata& execution_metadata) const;

 private:
  enum class ContentType {
    kJson = 0,
    kProto,
    kBhttp,
  };
  ContentType GetContentType(
      const quiche::BinaryHttpRequest& deserialized_req) const;

  ContentType GetContentType(
      const std::multimap<grpc::string_ref, grpc::string_ref>& headers,
      ContentType default_content_type) const;

  absl::Status GetValuesHttp(
      RequestContextFactory& request_context_factory, std::string_view request,
      std::string& json_response, ExecutionMetadata& execution_metadata,
      ContentType content_type = ContentType::kJson) const;

  // On success, returns a BinaryHttpResponse with a successful response. The
  // reason that this is a separate function is so that the error status
  // returned from here can be encoded as a BinaryHTTP response code. So even if
  // this function fails, the final grpc code may still be ok.
  absl::StatusOr<quiche::BinaryHttpResponse>
  BuildSuccessfulGetValuesBhttpResponse(
      RequestContextFactory& request_context_factory,
      std::string_view bhttp_request_body,
      ExecutionMetadata& execution_metadata) const;

  // Returns error only if the response cannot be serialized into Binary HTTP
  // response. For all other failures, the error status will be inside the
  // Binary HTTP message.
  absl::Status BinaryHttpGetValues(
      RequestContextFactory& request_context_factory,
      std::string_view bhttp_request_body, std::string& response,
      ExecutionMetadata& execution_metadata) const;

  // Invokes UDF to process one partition.
  absl::Status ProcessOnePartition(
      const RequestContextFactory& request_context_factory,
      const google::protobuf::Struct& req_metadata,
      const v2::RequestPartition& req_partition,
      v2::ResponsePartition& resp_partition,
      ExecutionMetadata& execution_metadata) const;

  const UdfClient& udf_client_;
  std::function<CompressionGroupConcatenator::FactoryFunctionType>
      create_compression_group_concatenator_;
  privacy_sandbox::server_common::KeyFetcherManagerInterface&
      key_fetcher_manager_;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_REQUEST_HANDLER_GET_VALUES_V2_HANDLER_H_
