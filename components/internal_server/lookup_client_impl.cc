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
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "components/internal_server/constants.h"
#include "components/internal_server/lookup.grpc.pb.h"
#include "components/internal_server/lookup_client.h"
#include "glog/logging.h"
#include "grpcpp/grpcpp.h"
#include "src/cpp/telemetry/telemetry.h"

ABSL_FLAG(absl::Duration, internal_lookup_deadline_duration,
          absl::Milliseconds(50),
          "Internal lookup RPC deadline. Default value is 50 milliseconds");

namespace kv_server {
namespace {

using privacy_sandbox::server_common::GetTracer;

class LookupClientImpl : public LookupClient {
 public:
  LookupClientImpl(const LookupClientImpl&) = delete;
  LookupClientImpl& operator=(const LookupClientImpl&) = delete;

  explicit LookupClientImpl(std::string_view server_address)
      : stub_(InternalLookupService::NewStub(grpc::CreateChannel(
            std::string(server_address), grpc::InsecureChannelCredentials()))) {
  }

  absl::StatusOr<InternalLookupResponse> GetValues(
      const std::vector<std::string>& keys) const override {
    InternalLookupRequest request;
    (*request.mutable_keys()) = {keys.begin(), keys.end()};

    InternalLookupResponse response;
    grpc::ClientContext context;
    absl::Duration deadline =
        absl::GetFlag(FLAGS_internal_lookup_deadline_duration);
    context.set_deadline(
        gpr_time_add(gpr_now(GPR_CLOCK_MONOTONIC),
                     gpr_time_from_millis(absl::ToInt64Milliseconds(deadline),
                                          GPR_TIMESPAN)));
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

std::unique_ptr<LookupClient> LookupClient::Create(
    std::string_view server_address) {
  return std::make_unique<LookupClientImpl>(server_address);
}

}  // namespace kv_server
