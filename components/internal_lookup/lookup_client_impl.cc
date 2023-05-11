// Copyright 2022 Google LLC
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
#include "absl/strings/str_cat.h"
#include "components/internal_lookup/constants.h"
#include "components/internal_lookup/lookup.grpc.pb.h"
#include "components/internal_lookup/lookup_client.h"
#include "glog/logging.h"
#include "grpcpp/grpcpp.h"
#include "src/cpp/telemetry/telemetry.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::GetTracer;

constexpr char kInternalLookupClientSpan[] = "InternalLookupClient";

class LookupClientImpl : public LookupClient {
 public:
  LookupClientImpl(const LookupClientImpl&) = delete;
  LookupClientImpl& operator=(const LookupClientImpl&) = delete;

  LookupClientImpl()
      : stub_(InternalLookupService::NewStub(
            grpc::CreateChannel(kInternalLookupServerAddress,
                                grpc::InsecureChannelCredentials()))) {}

  absl::StatusOr<InternalLookupResponse> GetValues(
      const std::vector<std::string>& keys) const override {
    auto span = GetTracer()->StartSpan(kInternalLookupClientSpan);
    auto scope = opentelemetry::trace::Scope(span);

    InternalLookupRequest request;
    (*request.mutable_keys()) = {keys.begin(), keys.end()};

    InternalLookupResponse response;
    grpc::ClientContext context;
    grpc::Status status = stub_->InternalLookup(&context, request, &response);

    if (status.ok()) {
      return response;
    }

    LOG(ERROR) << status.error_code() << ": " << status.error_message();
    // Return an absl status from the gRPC status
    return absl::Status((absl::StatusCode)status.error_code(),
                        status.error_message());
  }

 private:
  std::unique_ptr<InternalLookupService::Stub> stub_;
};

}  // namespace

const LookupClient& LookupClient::GetSingleton() {
  static const LookupClient* const kInstance = new LookupClientImpl();
  return *kInstance;
}

}  // namespace kv_server
