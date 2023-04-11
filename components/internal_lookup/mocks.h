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

#ifndef COMPONENTS_INTERNAL_LOOKUP_MOCKS_H_
#define COMPONENTS_INTERNAL_LOOKUP_MOCKS_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "components/internal_lookup/lookup.grpc.pb.h"
#include "components/internal_lookup/lookup_client.h"
#include "gmock/gmock.h"

namespace kv_server {

class MockLookupClient : public LookupClient {
 public:
  MOCK_METHOD((absl::StatusOr<InternalLookupResponse>), GetValues,
              (const std::vector<std::string>&), (const, override));
};

}  // namespace kv_server

#endif  // COMPONENTS_INTERNAL_LOOKUP_MOCKS_H_
