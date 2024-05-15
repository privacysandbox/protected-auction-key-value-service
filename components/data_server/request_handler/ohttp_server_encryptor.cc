// Copyright 2023 Google LLC
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

#include "components/data_server/request_handler/ohttp_server_encryptor.h"

#include <utility>

#include "absl/log/log.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"

namespace kv_server {
absl::StatusOr<absl::string_view> OhttpServerEncryptor::DecryptRequest(
    absl::string_view encrypted_payload,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  const absl::StatusOr<uint8_t> maybe_req_key_id =
      quiche::ObliviousHttpHeaderKeyConfig::
          ParseKeyIdFromObliviousHttpRequestPayload(encrypted_payload);
  if (!maybe_req_key_id.ok()) {
    return absl::InternalError(absl::StrCat(
        "Unable to get OHTTP key id: ", maybe_req_key_id.status().message()));
  }
  const auto maybe_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      *maybe_req_key_id, kKEMParameter, kKDFParameter, kAEADParameter);
  if (!maybe_config.ok()) {
    return absl::InternalError(absl::StrCat(
        "Unable to build OHTTP config: ", maybe_req_key_id.status().message()));
  }
  auto private_key_id = std::to_string(*maybe_req_key_id);

  PS_VLOG(9, log_context) << "Decrypting for the public key id: "
                          << private_key_id;
  auto private_key = key_fetcher_manager_.GetPrivateKey(private_key_id);
  if (!private_key.has_value()) {
    const std::string error = absl::StrCat(
        "Unable to retrieve private key for key ID: ", *maybe_req_key_id);
    PS_LOG(ERROR, log_context) << error;
    return absl::InternalError(error);
  }

  auto maybe_ohttp_gateway = quiche::ObliviousHttpGateway::Create(
      private_key->private_key, *maybe_config);
  if (!maybe_ohttp_gateway.ok()) {
    return maybe_ohttp_gateway.status();
  }
  ohttp_gateway_ = std::move(*maybe_ohttp_gateway);
  auto decrypted_request_maybe =
      ohttp_gateway_->DecryptObliviousHttpRequest(encrypted_payload);
  if (!decrypted_request_maybe.ok()) {
    return decrypted_request_maybe.status();
  }
  decrypted_request_ = std::move(*decrypted_request_maybe);
  return decrypted_request_->GetPlaintextData();
}

absl::StatusOr<std::string> OhttpServerEncryptor::EncryptResponse(
    std::string payload,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  if (!ohttp_gateway_.has_value() || !decrypted_request_.has_value()) {
    return absl::InternalError(
        "Emtpy `ohttp_gateway_` or `decrypted_request_`. You should call "
        "`ServerDecryptRequest` first");
  }
  auto server_request_context = std::move(*decrypted_request_).ReleaseContext();
  const auto encapsulate_resp = ohttp_gateway_->CreateObliviousHttpResponse(
      std::move(payload), server_request_context);
  if (!encapsulate_resp.ok()) {
    return absl::InternalError(
        std::string(encapsulate_resp.status().message()));
  }
  return encapsulate_resp->EncapsulateAndSerialize();
}
}  // namespace kv_server
