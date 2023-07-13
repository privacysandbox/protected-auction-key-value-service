/*
 * Copyright 2022 Google LLC
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

#ifndef COMPONENTS_INTERNAL_SERVER_LOOKUP_CLIENT_H_
#define COMPONENTS_INTERNAL_SERVER_LOOKUP_CLIENT_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "components/internal_server/lookup.grpc.pb.h"

namespace kv_server {

// TOOD(b/261564359): Create a pool of gRPC channels
// Synchronous client for internal key value lookup service
class LookupClient {
 public:
  virtual ~LookupClient() = default;

  // Calls the internal lookup server with the given keys.
  virtual absl::StatusOr<InternalLookupResponse> GetValues(
      const std::vector<std::string>& keys) const = 0;

  static std::unique_ptr<LookupClient> Create(std::string_view server_address);
};

}  // namespace kv_server

#endif  // COMPONENTS_INTERNAL_SERVER_LOOKUP_CLIENT_H_
