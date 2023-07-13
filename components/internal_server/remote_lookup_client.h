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

#ifndef COMPONENTS_INTERNAL_SERVER_REMOTE_LOOKUP_CLIENT_H_
#define COMPONENTS_INTERNAL_SERVER_REMOTE_LOOKUP_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "components/internal_server/lookup.grpc.pb.h"
#include "src/cpp/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"

namespace kv_server {

class RemoteLookupClient {
 public:
  virtual ~RemoteLookupClient() = default;
  // Calls the remote internal lookup server with the given keys.
  // Pads the request size with padding_length.
  // Note that we need to pass in a `serialized_message` here because we need to
  // figure out the correct padding length across multiple requests. That helps
  // with preventing double serialization.
  virtual absl::StatusOr<InternalLookupResponse> GetValues(
      std::string_view serialized_message, int32_t padding_length) const = 0;
  virtual std::string_view GetIpAddress() const = 0;
  static std::unique_ptr<RemoteLookupClient> Create(
      std::string ip_address,
      privacy_sandbox::server_common::KeyFetcherManagerInterface&
          key_fetcher_manager);
  static std::unique_ptr<RemoteLookupClient> Create(
      std::unique_ptr<InternalLookupService::Stub> stub,
      privacy_sandbox::server_common::KeyFetcherManagerInterface&
          key_fetcher_manager);
};

}  // namespace kv_server

#endif  // COMPONENTS_INTERNAL_SERVER_REMOTE_LOOKUP_CLIENT_H_
