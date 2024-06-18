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

#ifndef COMPONENTS_DATA_SERVER_CACHE_MOCKS_H_
#define COMPONENTS_DATA_SERVER_CACHE_MOCKS_H_

#include <memory>
#include <string>

#include "components/container/thread_safe_hash_map.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/uint32_value_set.h"
#include "gmock/gmock.h"

namespace kv_server {

MATCHER_P2(KVPairEq, key, value, "") {
  return testing::ExplainMatchResult(testing::Pair(key, value), arg,
                                     result_listener);
}

class MockCache : public Cache {
 public:
  MOCK_METHOD((absl::flat_hash_map<std::string, std::string>), GetKeyValuePairs,
              (const RequestContext&,
               const absl::flat_hash_set<std::string_view>&),
              (const, override));
  MOCK_METHOD((std::unique_ptr<GetKeyValueSetResult>), GetKeyValueSet,
              (const RequestContext&,
               const absl::flat_hash_set<std::string_view>&),
              (const, override));
  MOCK_METHOD((std::unique_ptr<GetKeyValueSetResult>), GetUInt32ValueSet,
              (const RequestContext&,
               const absl::flat_hash_set<std::string_view>&),
              (const, override));
  MOCK_METHOD(void, UpdateKeyValue,
              (privacy_sandbox::server_common::log::PSLogContext&,
               std::string_view, std::string_view, int64_t, std::string_view),
              (override));
  MOCK_METHOD(void, UpdateKeyValueSet,
              (privacy_sandbox::server_common::log::PSLogContext&,
               std::string_view, absl::Span<std::string_view>, int64_t,
               std::string_view),
              (override));
  MOCK_METHOD(void, UpdateKeyValueSet,
              (privacy_sandbox::server_common::log::PSLogContext&,
               std::string_view, absl::Span<uint32_t>, int64_t,
               std::string_view),
              (override));
  MOCK_METHOD(void, DeleteValuesInSet,
              (privacy_sandbox::server_common::log::PSLogContext&,
               std::string_view, absl::Span<std::string_view>, int64_t,
               std::string_view),
              (override));
  MOCK_METHOD(void, DeleteValuesInSet,
              (privacy_sandbox::server_common::log::PSLogContext&,
               std::string_view, absl::Span<uint32_t>, int64_t,
               std::string_view),
              (override));
  MOCK_METHOD(void, DeleteKey,
              (privacy_sandbox::server_common::log::PSLogContext&,
               std::string_view, int64_t, std::string_view),
              (override));
  MOCK_METHOD(void, RemoveDeletedKeys,
              (privacy_sandbox::server_common::log::PSLogContext&, int64_t,
               std::string_view),
              (override));
};

class MockGetKeyValueSetResult : public GetKeyValueSetResult {
 public:
  MOCK_METHOD((absl::flat_hash_set<std::string_view>), GetValueSet,
              (std::string_view), (const, override));
  MOCK_METHOD(void, AddKeyValueSet,
              (std::string_view, absl::flat_hash_set<std::string_view>,
               std::unique_ptr<absl::ReaderMutexLock>),
              (override));
  MOCK_METHOD((const UInt32ValueSet*), GetUInt32ValueSet, (std::string_view),
              (const override));
  MOCK_METHOD(
      void, AddUInt32ValueSet,
      (std::string_view,
       (ThreadSafeHashMap<std::string, UInt32ValueSet>::ConstLockedNodePtr)),
      (override));
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_CACHE_MOCKS_H_
