/*
 * Copyright 2023 Google LLC
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

#ifndef PUBLIC_QUERY_CPP_GRPC_CLIENT_H_
#define PUBLIC_QUERY_CPP_GRPC_CLIENT_H_

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "grpcpp/grpcpp.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"

namespace kv_server {

// Basic gRPC client. Sends requests synchronously.
//
// Example usage:
//
// std::unique_ptr<v2::KeyValueService::Stub> stub =
// GrpcClient::CreateStub("https://example.com",
// grpc::SslCredentials(grpc::SslCredentialsOptions()));
//
// GrpcClient client(*stub);
// auto maybe_response = client.GetValues(req);
// if (maybe_response.ok()) {
//   auto response = maybe_response.value();
// }
class GrpcClient {
 public:
  static std::unique_ptr<v2::KeyValueService::Stub> CreateStub(
      const std::string& key_value_server_address,
      std::shared_ptr<grpc::ChannelCredentials> credentials);

  // `stub` must exist longer than this class object.
  explicit GrpcClient(v2::KeyValueService::StubInterface& stub,
                      absl::Duration deadline = absl::Seconds(50))
      : stub_(stub), deadline_(deadline) {}

  // Sends a request to the key value server at the address passed during the
  // class construction. Waits for the response and returns the response or an
  // error.
  absl::StatusOr<v2::GetValuesResponse> GetValues(
      const v2::GetValuesRequest& request) const;

 private:
  v2::KeyValueService::StubInterface& stub_;
  absl::Duration deadline_;
};

}  // namespace kv_server

#endif  // PUBLIC_QUERY_CPP_GRPC_CLIENT_H_
