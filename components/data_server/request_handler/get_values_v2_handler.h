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

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/request_handler/compression.h"
#include "components/udf/udf_client.h"
#include "grpcpp/grpcpp.h"
#include "nlohmann/json.hpp"
#include "public/query/v2/get_values_v2.grpc.pb.h"
#include "quiche/binary_http/binary_http_message.h"
#include "src/cpp/encryption/key_fetcher/src/key_fetcher_manager.h"
#include "src/cpp/telemetry/metrics_recorder.h"

namespace kv_server {

// Handles the request family of *GetValues.
// See the Service proto definition for details.
class GetValuesV2Handler {
 public:
  // Accepts a functor to create compression blob builder for testing purposes.
  explicit GetValuesV2Handler(
      const UdfClient& udf_client,
      privacy_sandbox::server_common::MetricsRecorder& metrics_recorder,
      privacy_sandbox::server_common::KeyFetcherManagerInterface&
          key_fetcher_manager,
      std::function<CompressionGroupConcatenator::FactoryFunctionType>
          create_compression_group_concatenator =
              &CompressionGroupConcatenator::Create)
      : udf_client_(udf_client),
        metrics_recorder_(metrics_recorder),
        create_compression_group_concatenator_(
            std::move(create_compression_group_concatenator)),
        key_fetcher_manager_(key_fetcher_manager) {}

  absl::StatusOr<nlohmann::json> GetValuesJsonResponse(
      const v2::GetValuesHttpRequest& request) const;

  grpc::Status GetValuesHttp(const v2::GetValuesHttpRequest& request,
                             google::api::HttpBody* response) const;

  grpc::Status BinaryHttpGetValues(
      const v2::BinaryHttpGetValuesRequest& request,
      google::api::HttpBody* response) const;

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
  // AEAD: AES-128-GCM 0X0001
  // (https://github.com/WICG/turtledove/blob/main/FLEDGE_Key_Value_Server_API.md#encryption)
  grpc::Status ObliviousGetValues(const v2::ObliviousGetValuesRequest& request,
                                  google::api::HttpBody* response) const;

  // Given a list of compression group objects, create a JSON object to
  // represent the list.
  static nlohmann::json BuildCompressionGroupsForDebugging(
      std::vector<nlohmann::json> compression_groups);

 private:
  // On success, returns a BinaryHttpResponse with a successful response. The
  // reason that this is a separate function is so that the error status
  // returned from here can be encoded as a BinaryHTTP response code. So even if
  // this function fails, the final grpc code may still be ok.
  absl::StatusOr<quiche::BinaryHttpResponse>
  BuildSuccessfulGetValuesBhttpResponse(
      std::string_view bhttp_request_body) const;

  // Returns error only if the response cannot be serialized into Binary HTTP
  // response. For all other failures, the error status will be inside the
  // Binary HTTP message.
  grpc::Status BinaryHttpGetValues(std::string_view bhttp_request_body,
                                   std::string& response) const;

  const UdfClient& udf_client_;
  std::function<CompressionGroupConcatenator::FactoryFunctionType>
      create_compression_group_concatenator_;
  privacy_sandbox::server_common::MetricsRecorder& metrics_recorder_;
  privacy_sandbox::server_common::KeyFetcherManagerInterface&
      key_fetcher_manager_;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_REQUEST_HANDLER_GET_VALUES_V2_HANDLER_H_
