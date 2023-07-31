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
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "components/data_server/request_handler/ohttp_client_encryptor.h"
#include "components/internal_server/constants.h"
#include "components/internal_server/lookup.grpc.pb.h"
#include "components/internal_server/remote_lookup_client.h"
#include "components/internal_server/string_padder.h"
#include "glog/logging.h"
#include "grpcpp/grpcpp.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::ScopeLatencyRecorder;

constexpr char kEncryptionFailure[] = "EncryptionFailure";
constexpr char kSecureLookupFailure[] = "SecureLookupFailure";
constexpr char kDecryptionFailure[] = "DecryptionFailure";
constexpr char kRemoteLookupGetValues[] = "RemoteLookupGetValues";

class RemoteLookupClientImpl : public RemoteLookupClient {
 public:
  RemoteLookupClientImpl(const RemoteLookupClientImpl&) = delete;
  RemoteLookupClientImpl& operator=(const RemoteLookupClientImpl&) = delete;

  explicit RemoteLookupClientImpl(
      std::string ip_address,
      privacy_sandbox::server_common::KeyFetcherManagerInterface&
          key_fetcher_manager,
      privacy_sandbox::server_common::MetricsRecorder& metrics_recorder)
      : ip_address_(
            absl::StrFormat("%s:%s", ip_address, kRemoteLookupServerPort)),
        stub_(InternalLookupService::NewStub(grpc::CreateChannel(
            ip_address_, grpc::InsecureChannelCredentials()))),
        key_fetcher_manager_(key_fetcher_manager),
        metrics_recorder_(metrics_recorder) {}

  explicit RemoteLookupClientImpl(
      std::unique_ptr<InternalLookupService::Stub> stub,
      privacy_sandbox::server_common::KeyFetcherManagerInterface&
          key_fetcher_manager,
      privacy_sandbox::server_common::MetricsRecorder& metrics_recorder)
      : stub_(std::move(stub)),
        key_fetcher_manager_(key_fetcher_manager),
        metrics_recorder_(metrics_recorder) {}

  absl::StatusOr<InternalLookupResponse> GetValues(
      std::string_view serialized_message,
      int32_t padding_length) const override {
    ScopeLatencyRecorder latency_recorder(std::string(kRemoteLookupGetValues),
                                          metrics_recorder_);
    OhttpClientEncryptor encryptor(key_fetcher_manager_);
    auto encrypted_padded_serialized_request_maybe =
        encryptor.EncryptRequest(Pad(serialized_message, padding_length));
    if (!encrypted_padded_serialized_request_maybe.ok()) {
      metrics_recorder_.IncrementEventCounter(kEncryptionFailure);
      return encrypted_padded_serialized_request_maybe.status();
    }
    SecureLookupRequest secure_lookup_request;
    secure_lookup_request.set_ohttp_request(
        *encrypted_padded_serialized_request_maybe);
    SecureLookupResponse secure_response;
    grpc::ClientContext context;
    grpc::Status status =
        stub_->SecureLookup(&context, secure_lookup_request, &secure_response);
    if (!status.ok()) {
      metrics_recorder_.IncrementEventCounter(kSecureLookupFailure);
      LOG(ERROR) << status.error_code() << ": " << status.error_message();
      return absl::Status((absl::StatusCode)status.error_code(),
                          status.error_message());
    }
    InternalLookupResponse response;
    if (secure_response.ohttp_response().empty()) {
      // we cannot decrypt an empty response. Note, that soon we will add logic
      // to pad responses, so this branch will never be hit.
      return response;
    }
    auto decrypted_response_maybe =
        encryptor.DecryptResponse(std::move(secure_response.ohttp_response()));
    if (!decrypted_response_maybe.ok()) {
      metrics_recorder_.IncrementEventCounter(kDecryptionFailure);
      return decrypted_response_maybe.status();
    }

    if (!response.ParseFromString(
            decrypted_response_maybe->GetPlaintextData())) {
      return absl::InvalidArgumentError("Failed parsing the response.");
    }
    return response;
  }

  std::string_view GetIpAddress() const override { return ip_address_; }

 private:
  const std::string ip_address_;
  std::unique_ptr<InternalLookupService::Stub> stub_;
  privacy_sandbox::server_common::KeyFetcherManagerInterface&
      key_fetcher_manager_;
  privacy_sandbox::server_common::MetricsRecorder& metrics_recorder_;
};

}  // namespace

std::unique_ptr<RemoteLookupClient> RemoteLookupClient::Create(
    std::string ip_address,
    privacy_sandbox::server_common::KeyFetcherManagerInterface&
        key_fetcher_manager,
    privacy_sandbox::server_common::MetricsRecorder& metrics_recorder) {
  return std::make_unique<RemoteLookupClientImpl>(
      std::move(ip_address), key_fetcher_manager, metrics_recorder);
}
std::unique_ptr<RemoteLookupClient> RemoteLookupClient::Create(
    std::unique_ptr<InternalLookupService::Stub> stub,
    privacy_sandbox::server_common::KeyFetcherManagerInterface&
        key_fetcher_manager,
    privacy_sandbox::server_common::MetricsRecorder& metrics_recorder) {
  return std::make_unique<RemoteLookupClientImpl>(
      std::move(stub), key_fetcher_manager, metrics_recorder);
}

}  // namespace kv_server
