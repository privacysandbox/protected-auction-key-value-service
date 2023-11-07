/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "public/query/cpp/grpc_client.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "grpcpp/support/status.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"

namespace kv_server {

std::unique_ptr<v2::KeyValueService::Stub> GrpcClient::CreateStub(
    const std::string& key_value_server_address,
    std::shared_ptr<grpc::ChannelCredentials> credentials) {
  return v2::KeyValueService::NewStub(
      grpc::CreateChannel(key_value_server_address, std::move(credentials)));
}

absl::StatusOr<v2::GetValuesResponse> GrpcClient::GetValues(
    const v2::GetValuesRequest& request) const {
  v2::GetValuesResponse response;
  grpc::ClientContext context;
  context.set_deadline(
      gpr_time_add(gpr_now(GPR_CLOCK_MONOTONIC),
                   gpr_time_from_millis(absl::ToInt64Milliseconds(deadline_),
                                        GPR_TIMESPAN)));
  grpc::Status status = stub_.GetValues(&context, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << status.error_code() << ": " << status.error_message();
    return absl::Status((absl::StatusCode)status.error_code(),
                        status.error_message());
  }
  return response;
}

}  // namespace kv_server
