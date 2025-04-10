/*
 * Copyright 2025 Google LLC
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

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/flags.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "components/data/converters/cbor_converter.h"
#include "components/data_server/request_handler/encryption/ohttp_client_encryptor.h"
#include "google/protobuf/util/json_util.h"
#include "nlohmann/json.hpp"
#include "public/constants.h"
#include "public/query/v2/get_values_v2.pb.h"
#include "src/communication/encoding_utils.h"
#include "src/communication/framing_utils.h"
#include "src/encryption/key_fetcher/fake_key_fetcher_manager.h"

ABSL_FLAG(
    std::string, input_json_path, "",
    "Path to the input JSON file containing the plaintext request structure.");
ABSL_FLAG(std::string, output_json_path, "",
          "Path to the output JSON file to write the encrypted request.");

namespace kv_server {
namespace {

absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey>
GetPublicKey() {
  auto key_fetcher_manager = std::make_unique<
      ::privacy_sandbox::server_common::FakeKeyFetcherManager>();
  auto maybe_public_key = key_fetcher_manager->GetPublicKey(
      privacy_sandbox::server_common::CloudPlatform::kLocal);
  if (!maybe_public_key.ok()) {
    const std::string error =
        absl::StrCat("Could not get public key to use for HPKE encryption:",
                     maybe_public_key.status().message());
    LOG(ERROR) << error;
    return absl::InternalError(error);
  }
  LOG(INFO) << "Using hardcoded public key for encryption.";
  return maybe_public_key.value();
}

absl::Status EncryptRequestAndWriteOutput(
    const std::string& input_json_content,
    std::unique_ptr<google::cmrt::sdk::public_key_service::v1::PublicKey>&
        public_key,
    const std::string& output_path) {
  std::string serialized_req_cbor =
      V2GetValuesRequestJsonStringCborEncode(input_json_content).value();
  LOG(INFO) << "Successfully serialized JSON request to CBOR.";

  auto encoded_data_size = privacy_sandbox::server_common::GetEncodedDataSize(
      serialized_req_cbor.size(), kMinResponsePaddingBytes);
  auto maybe_padded_request =
      privacy_sandbox::server_common::EncodeResponsePayload(
          privacy_sandbox::server_common::CompressionType::kUncompressed,
          std::move(serialized_req_cbor), encoded_data_size);
  if (!maybe_padded_request.ok()) {
    LOG(ERROR) << "Padding failed: " << maybe_padded_request.status().message();
    return maybe_padded_request.status();
  }
  OhttpClientEncryptor encryptor(*public_key);
  auto encrypted_serialized_request_maybe =
      encryptor.EncryptRequest(*maybe_padded_request);
  if (!encrypted_serialized_request_maybe.ok()) {
    LOG(ERROR) << "OHTTP request encryption failed: "
               << encrypted_serialized_request_maybe.status();
    return encrypted_serialized_request_maybe.status();
  }
  LOG(INFO) << "Successfully encrypted request.";

  std::string encrypted_base64;
  absl::Base64Escape(*encrypted_serialized_request_maybe, &encrypted_base64);
  LOG(INFO) << "Base64 encoded encrypted request.";

  nlohmann::json output_json;
  output_json["raw_body"]["data"] = encrypted_base64;
  std::ofstream output_file(output_path);
  if (!output_file.is_open()) {
    return absl::InternalError(
        absl::StrCat("Failed to open output file: ", output_path));
  }
  output_file << output_json.dump(2);
  output_file.close();
  LOG(INFO) << "Successfully wrote encrypted request to: " << output_path;
  return absl::OkStatus();
}

}  // namespace
}  // namespace kv_server

int main(int argc, char** argv) {
  const std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  std::string input_path = absl::GetFlag(FLAGS_input_json_path);
  std::string output_path = absl::GetFlag(FLAGS_output_json_path);
  if (input_path.empty()) {
    LOG(ERROR) << "--input_json_path flag is required.";
    return 1;
  }
  if (output_path.empty()) {
    LOG(ERROR) << "--output_json_path flag is required.";
    return 1;
  }

  // Get Public Key
  auto maybe_public_key = kv_server::GetPublicKey();
  if (!maybe_public_key.ok()) {
    LOG(ERROR) << "Failed to get public key: " << maybe_public_key.status();
    return 1;
  }
  auto public_key =
      std::make_unique<google::cmrt::sdk::public_key_service::v1::PublicKey>(
          maybe_public_key.value());
  LOG(INFO) << "Successfully obtained public key with ID: "
            << public_key->key_id();

  // Read Input JSON File
  LOG(INFO) << "Reading input JSON from: " << input_path;
  std::ifstream input_file(input_path);
  if (!input_file.is_open()) {
    LOG(ERROR) << "Failed to open input file: " << input_path;
    return 1;
  }
  std::stringstream buffer;
  buffer << input_file.rdbuf();
  std::string input_json_content = buffer.str();
  input_file.close();

  if (input_json_content.empty()) {
    LOG(ERROR) << "Input JSON file is empty: " << input_path;
    return 1;
  }

  // Perform Encryption and Write Output
  absl::Status process_status = kv_server::EncryptRequestAndWriteOutput(
      input_json_content, public_key, output_path);

  if (!process_status.ok()) {
    LOG(ERROR) << "Encryption process failed: " << process_status;
    return 1;
  }

  LOG(INFO) << "Encryption successful. Output written to " << output_path;
  return 0;
}
