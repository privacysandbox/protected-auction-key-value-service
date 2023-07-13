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

#ifndef COMPONENTS_INTERNAL_SERVER_RUN_QUERY_CLIENT_H_
#define COMPONENTS_INTERNAL_SERVER_RUN_QUERY_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "components/internal_server/lookup.grpc.pb.h"

namespace kv_server {

// TOOD(b/261564359): Create a pool of gRPC channels and combine internal
// clients.
// Synchronous client for internal run query service
class RunQueryClient {
 public:
  virtual ~RunQueryClient() = default;

  // Calls the internal run query server.
  virtual absl::StatusOr<InternalRunQueryResponse> RunQuery(
      std::string query) const = 0;

  // If this client is called as part of the UDF hook, it must be constructed
  // after the fork.
  static std::unique_ptr<RunQueryClient> Create(
      std::string_view server_address);
};

}  // namespace kv_server

#endif  // COMPONENTS_INTERNAL_SERVER_RUN_QUERY_CLIENT_H_
