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

#include <string_view>

#include "components/data_server/request_handler/encryption/ohttp_client_encryptor.h"
#include "components/data_server/request_handler/encryption/ohttp_server_encryptor.h"
#include "gtest/gtest.h"
#include "src/encryption/key_fetcher/fake_key_fetcher_manager.h"
#include "src/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::CloudPlatform::kLocal;

TEST(OhttpEncryptorTest, FullCircleSuccess) {
  const std::string kTestRequest = "request to encrypt";
  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager;
  auto public_key = fake_key_fetcher_manager.GetPublicKey(kLocal);
  OhttpClientEncryptor client_encryptor(public_key.value());
  OhttpServerEncryptor server_encryptor(fake_key_fetcher_manager);
  auto request_encrypted_status = client_encryptor.EncryptRequest(kTestRequest);
  ASSERT_TRUE(request_encrypted_status.ok());
  auto request_decrypted_status =
      server_encryptor.DecryptRequest(*request_encrypted_status);
  ASSERT_TRUE(request_decrypted_status.ok());
  EXPECT_EQ(kTestRequest, *request_decrypted_status);

  const std::string kTestResponse = "response to encrypt";
  auto response_encrypted_status =
      server_encryptor.EncryptResponse(kTestResponse);
  ASSERT_TRUE(response_encrypted_status.ok());
  auto response_decrypted_status =
      client_encryptor.DecryptResponse(*response_encrypted_status);
  ASSERT_TRUE(response_decrypted_status.ok());
  EXPECT_EQ(kTestResponse, *response_decrypted_status);
}

TEST(OhttpEncryptorTest, ServerDecryptRequestFails) {
  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager;
  OhttpServerEncryptor server_encryptor(fake_key_fetcher_manager);
  auto request_decrypted_status = server_encryptor.DecryptRequest("garbage");
  ASSERT_FALSE(request_decrypted_status.ok());
}

TEST(OhttpEncryptorTest, ClientDecryptFails) {
  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager;
  const std::string kTestRequest = "request to encrypt";
  auto public_key = fake_key_fetcher_manager.GetPublicKey(kLocal);
  OhttpClientEncryptor client_encryptor(public_key.value());
  auto request_encrypted_status = client_encryptor.EncryptRequest(kTestRequest);
  ASSERT_TRUE(request_encrypted_status.ok());
  auto response_decrypted_status = client_encryptor.DecryptResponse("garbage");
  ASSERT_FALSE(response_decrypted_status.ok());
}

TEST(OhttpEncryptorTest, ServerEncryptResponseFails) {
  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager;
  const std::string kTestRequest = "request to encrypt";
  OhttpServerEncryptor server_encryptor(fake_key_fetcher_manager);
  auto request_encrypted_status =
      server_encryptor.EncryptResponse(kTestRequest);
  ASSERT_FALSE(request_encrypted_status.ok());
  EXPECT_EQ(
      "Emtpy `ohttp_gateway_` or `decrypted_request_`. You should call "
      "`ServerDecryptRequest` first",
      request_encrypted_status.status().message());
}

TEST(OhttpEncryptorTest, ClientDecryptResponseFails) {
  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager;
  const std::string kTestRequest = "request to decrypt";
  auto public_key = fake_key_fetcher_manager.GetPublicKey(kLocal);
  OhttpClientEncryptor client_encryptor(public_key.value());
  auto request_encrypted_status =
      client_encryptor.DecryptResponse(kTestRequest);
  ASSERT_FALSE(request_encrypted_status.ok());
  EXPECT_EQ(
      "Emtpy `http_request_context_`. You should call "
      "`ClientEncryptRequest` first",
      request_encrypted_status.status().message());
}

}  // namespace
}  // namespace kv_server
