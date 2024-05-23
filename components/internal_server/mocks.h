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

#ifndef COMPONENTS_INTERNAL_SERVER_MOCKS_H_
#define COMPONENTS_INTERNAL_SERVER_MOCKS_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "components/internal_server/lookup.h"
#include "components/internal_server/lookup.pb.h"
#include "components/internal_server/remote_lookup_client.h"
#include "gmock/gmock.h"

namespace kv_server {

class MockRemoteLookupClient : public RemoteLookupClient {
 public:
  MockRemoteLookupClient() : RemoteLookupClient() {}
  MOCK_METHOD(absl::StatusOr<InternalLookupResponse>, GetValues,
              (const RequestContext& request_context,
               std::string_view serialized_message, int32_t padding_length),
              (const, override));
  MOCK_METHOD(std::string_view, GetIpAddress, (), (const, override));
};

class MockLookup : public Lookup {
 public:
  MOCK_METHOD(absl::StatusOr<InternalLookupResponse>, GetKeyValues,
              (const RequestContext&,
               const absl::flat_hash_set<std::string_view>&),
              (const, override));
  MOCK_METHOD(absl::StatusOr<InternalLookupResponse>, GetKeyValueSet,
              (const RequestContext&,
               const absl::flat_hash_set<std::string_view>&),
              (const, override));
  MOCK_METHOD(absl::StatusOr<InternalLookupResponse>, GetUInt32ValueSet,
              (const RequestContext&,
               const absl::flat_hash_set<std::string_view>&),
              (const, override));
  MOCK_METHOD(absl::StatusOr<InternalRunQueryResponse>, RunQuery,
              (const RequestContext&, std::string query), (const, override));
  MOCK_METHOD(absl::StatusOr<InternalRunSetQueryIntResponse>, RunSetQueryInt,
              (const RequestContext&, std::string query), (const, override));
};

}  // namespace kv_server

#endif  // COMPONENTS_INTERNAL_SERVER_MOCKS_H_
