// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COMPONENTS_DATA_SERVER_REQUEST_HANDLER_OHTTP_CLIENT_ENCRYPTOR_H_
#define COMPONENTS_DATA_SERVER_REQUEST_HANDLER_OHTTP_CLIENT_ENCRYPTOR_H_

#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "public/constants.h"
#include "quiche/oblivious_http/oblivious_http_client.h"
#include "src/cpp/encryption/key_fetcher/src/key_fetcher_manager.h"

namespace kv_server {

// Handles client side encyption of requests and decryptions of responses.
// Not thread safe. Supports serial encryption/decryption per request.
class OhttpClientEncryptor {
 public:
  explicit OhttpClientEncryptor(
      privacy_sandbox::server_common::KeyFetcherManagerInterface&
          key_fetcher_manager)
      : key_fetcher_manager_(key_fetcher_manager) {
#if defined(CLOUD_PLATFORM_AWS)
    cloud_platform_ = privacy_sandbox::server_common::CloudPlatform::kAws;
#elif defined(CLOUD_PLATFORM_GCP)
    cloud_platform_ = privacy_sandbox::server_common::CloudPlatform::kGcp;
#endif
  }
  // Encrypts ougoing request.
  absl::StatusOr<std::string> EncryptRequest(std::string payload);
  // Decrypts incoming reponse. Since OHTTP is stateful, this method should be
  // called after EncryptRequest.
  absl::StatusOr<std::string> DecryptResponse(std::string encrypted_payload);

 private:
  ::privacy_sandbox::server_common::CloudPlatform cloud_platform_ =
      ::privacy_sandbox::server_common::CloudPlatform::kLocal;
  std::optional<quiche::ObliviousHttpClient> http_client_;
  std::optional<quiche::ObliviousHttpRequest::Context> http_request_context_;
  privacy_sandbox::server_common::KeyFetcherManagerInterface&
      key_fetcher_manager_;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_REQUEST_HANDLER_OHTTP_CLIENT_ENCRYPTOR_H_
