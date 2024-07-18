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

#ifndef COMPONENTS_INTERNAL_SERVER_LOOKUP_H_
#define COMPONENTS_INTERNAL_SERVER_LOOKUP_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "components/internal_server/lookup.pb.h"
#include "components/util/request_context.h"

namespace kv_server {

// Helper class for lookup.proto calls
class Lookup {
 public:
  virtual ~Lookup() = default;

  virtual absl::StatusOr<InternalLookupResponse> GetKeyValues(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& keys) const = 0;

  virtual absl::StatusOr<InternalLookupResponse> GetKeyValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const = 0;

  virtual absl::StatusOr<InternalLookupResponse> GetUInt32ValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const = 0;

  virtual absl::StatusOr<InternalLookupResponse> GetUInt64ValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const = 0;

  virtual absl::StatusOr<InternalRunQueryResponse> RunQuery(
      const RequestContext& request_context, std::string query) const = 0;

  virtual absl::StatusOr<InternalRunSetQueryUInt32Response> RunSetQueryUInt32(
      const RequestContext& request_context, std::string query) const = 0;

  virtual absl::StatusOr<InternalRunSetQueryUInt64Response> RunSetQueryUInt64(
      const RequestContext& request_context, std::string query) const = 0;
};

}  // namespace kv_server

#endif  // COMPONENTS_INTERNAL_SERVER_LOOKUP_H_
