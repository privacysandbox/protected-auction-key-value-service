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

#include "components/data_server/request_handler/ohttp_client_encryptor.h"

#include <utility>

#include "absl/strings/escaping.h"
#include "glog/logging.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"

namespace kv_server {
namespace {
absl::StatusOr<uint8_t> StringToUint8(absl::string_view str) {
  if (str.empty()) {
    return absl::InvalidArgumentError("Empty string.");
  }
  // SimpleAtoi doesn't support 8 bit conversion.
  uint8_t val8 = 0;
  uint32_t val = 0;
  if (absl::SimpleAtoi(str, &val) && (val8 = val) == val) {
    return val8;
  }
  return absl::InvalidArgumentError("String is not a uint8.");
}
}  // namespace

absl::StatusOr<std::string> OhttpClientEncryptor::EncryptRequest(
    std::string payload) {
  auto key = key_fetcher_manager_.GetPublicKey();
  if (!key.ok()) {
    const std::string error =
        absl::StrCat("Could not get public key to use for HPKE encryption: ",
                     key.status().message());
    LOG(ERROR) << error;
    return absl::InternalError(error);
  }
  auto key_id = StringToUint8(key->key_id());
  if (!key_id.ok()) {
    return key_id.status();
  }
  auto maybe_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      *key_id, kKEMParameter, kKDFParameter, kAEADParameter);
  if (!maybe_config.ok()) {
    return absl::InternalError(std::string(maybe_config.status().message()));
  }
  std::string public_key;
  VLOG(9) << "Encrypting with public key id: " << key->key_id()
          << " uint8 key id " << *key_id << "public key " << key->public_key();
  absl::Base64Unescape(key->public_key(), &public_key);
  auto http_client_maybe =
      quiche::ObliviousHttpClient::Create(public_key, *maybe_config);
  if (!http_client_maybe.ok()) {
    return absl::InternalError(
        std::string(http_client_maybe.status().message()));
  }
  http_client_ = std::move(*http_client_maybe);
  auto encrypted_req =
      http_client_->CreateObliviousHttpRequest(std::move(payload));
  if (!encrypted_req.ok()) {
    return absl::InternalError(std::string(encrypted_req.status().message()));
  }
  std::string serialized_encrypted_req =
      encrypted_req->EncapsulateAndSerialize();
  http_request_context_ = std::move(encrypted_req.value()).ReleaseContext();
  return serialized_encrypted_req;
}

absl::StatusOr<quiche::ObliviousHttpResponse>
OhttpClientEncryptor::DecryptResponse(std::string encrypted_payload) {
  if (!http_client_.has_value() || !http_request_context_.has_value()) {
    return absl::InternalError(
        "Emtpy `http_client_` or `http_request_context_`. You should call "
        "`ClientEncryptRequest` first");
  }
  auto decrypted_response = http_client_->DecryptObliviousHttpResponse(
      std::move(encrypted_payload), *http_request_context_);
  if (!decrypted_response.ok()) {
    return decrypted_response.status();
  }
  return *decrypted_response;
}
}  // namespace kv_server
