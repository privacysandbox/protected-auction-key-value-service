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

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "components/data_server/request_handler/encryption/ohttp_client_encryptor.h"
#include "components/internal_server/constants.h"
#include "components/internal_server/lookup.grpc.pb.h"
#include "components/internal_server/remote_lookup_client.h"
#include "components/internal_server/string_padder.h"
#include "grpcpp/grpcpp.h"

namespace kv_server {
namespace {

class RemoteLookupClientImpl : public RemoteLookupClient {
 public:
  RemoteLookupClientImpl(const RemoteLookupClientImpl&) = delete;
  RemoteLookupClientImpl& operator=(const RemoteLookupClientImpl&) = delete;

  explicit RemoteLookupClientImpl(
      std::string ip_address,
      privacy_sandbox::server_common::KeyFetcherManagerInterface&
          key_fetcher_manager)
      : ip_address_(
            absl::StrFormat("%s:%s", ip_address, kRemoteLookupServerPort)),
        stub_(InternalLookupService::NewStub(grpc::CreateChannel(
            ip_address_, grpc::InsecureChannelCredentials()))),
        key_fetcher_manager_(key_fetcher_manager) {}

  explicit RemoteLookupClientImpl(
      std::unique_ptr<InternalLookupService::Stub> stub,
      privacy_sandbox::server_common::KeyFetcherManagerInterface&
          key_fetcher_manager)
      : stub_(std::move(stub)), key_fetcher_manager_(key_fetcher_manager) {}

  absl::StatusOr<InternalLookupResponse> GetValues(
      const RequestContext& request_context,
      std::string_view serialized_message,
      int32_t padding_length) const override {
    ScopeLatencyMetricsRecorder<UdfRequestMetricsContext,
                                kRemoteLookupGetValuesLatencyInMicros>
        latency_recorder(request_context.GetUdfRequestMetricsContext());
    auto maybe_public_key =
        key_fetcher_manager_.GetPublicKey(GetCloudPlatform());
    if (!maybe_public_key.ok()) {
      const std::string error =
          absl::StrCat("Could not get public key to use for HPKE encryption:",
                       maybe_public_key.status().message());
      PS_LOG(ERROR, request_context.GetPSLogContext()) << error;
      return absl::InternalError(error);
    }
    OhttpClientEncryptor encryptor(maybe_public_key.value());
    auto encrypted_padded_serialized_request_maybe =
        encryptor.EncryptRequest(Pad(serialized_message, padding_length),
                                 request_context.GetPSLogContext());
    if (!encrypted_padded_serialized_request_maybe.ok()) {
      LogUdfRequestErrorMetric(request_context.GetUdfRequestMetricsContext(),
                               kRemoteRequestEncryptionFailure);
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
      LogUdfRequestErrorMetric(request_context.GetUdfRequestMetricsContext(),
                               kRemoteSecureLookupFailure);
      PS_LOG(ERROR, request_context.GetPSLogContext())
          << status.error_code() << ": " << status.error_message();
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
        encryptor.DecryptResponse(std::move(secure_response.ohttp_response()),
                                  request_context.GetPSLogContext());
    if (!decrypted_response_maybe.ok()) {
      LogUdfRequestErrorMetric(request_context.GetUdfRequestMetricsContext(),
                               kResponseEncryptionFailure);
      return decrypted_response_maybe.status();
    }
    if (!response.ParseFromString(*decrypted_response_maybe)) {
      return absl::InvalidArgumentError("Failed parsing the response.");
    }
    return response;
  }

  std::string_view GetIpAddress() const override { return ip_address_; }

 private:
  privacy_sandbox::server_common::CloudPlatform GetCloudPlatform() const {
#if defined(CLOUD_PLATFORM_AWS)
    return privacy_sandbox::server_common::CloudPlatform::kAws;
#elif defined(CLOUD_PLATFORM_GCP)
    return privacy_sandbox::server_common::CloudPlatform::kGcp;
#endif
    return privacy_sandbox::server_common::CloudPlatform::kLocal;
  }
  const std::string ip_address_;
  std::unique_ptr<InternalLookupService::Stub> stub_;
  privacy_sandbox::server_common::KeyFetcherManagerInterface&
      key_fetcher_manager_;
};

}  // namespace

std::unique_ptr<RemoteLookupClient> RemoteLookupClient::Create(
    std::string ip_address,
    privacy_sandbox::server_common::KeyFetcherManagerInterface&
        key_fetcher_manager) {
  return std::make_unique<RemoteLookupClientImpl>(std::move(ip_address),
                                                  key_fetcher_manager);
}
std::unique_ptr<RemoteLookupClient> RemoteLookupClient::Create(
    std::unique_ptr<InternalLookupService::Stub> stub,
    privacy_sandbox::server_common::KeyFetcherManagerInterface&
        key_fetcher_manager) {
  return std::make_unique<RemoteLookupClientImpl>(std::move(stub),
                                                  key_fetcher_manager);
}

}  // namespace kv_server
