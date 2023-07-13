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
#include <utility>
#include <vector>

#include "components/data_server/cache/cache.h"
#include "gmock/gmock.h"

namespace kv_server {

MATCHER_P2(KVPairEq, key, value, "") {
  return testing::ExplainMatchResult(testing::Pair(key, value), arg,
                                     result_listener);
}

class MockCache : public Cache {
 public:
  MOCK_METHOD((absl::flat_hash_map<std::string, std::string>), GetKeyValuePairs,
              (const std::vector<std::string_view>& key_list),
              (const, override));
  MOCK_METHOD((std::unique_ptr<GetKeyValueSetResult>), GetKeyValueSet,
              (const absl::flat_hash_set<std::string_view>&),
              (const, override));
  MOCK_METHOD(void, UpdateKeyValue,
              (std::string_view key, std::string_view value, int64_t ts),
              (override));
  MOCK_METHOD(void, UpdateKeyValueSet,
              (std::string_view key, absl::Span<std::string_view> value_set,
               int64_t logical_commit_time),
              (override));
  MOCK_METHOD(void, DeleteValuesInSet,
              (std::string_view key, absl::Span<std::string_view> value_set,
               int64_t logical_commit_time),
              (override));
  MOCK_METHOD(void, DeleteKey, (std::string_view key, int64_t ts), (override));
  MOCK_METHOD(void, RemoveDeletedKeys, (int64_t ts), (override));
};

class MockGetKeyValueSetResult : public GetKeyValueSetResult {
 public:
  MOCK_METHOD((absl::flat_hash_set<std::string_view>), GetValueSet,
              (std::string_view), (const, override));
  MOCK_METHOD(void, AddKeyValueSet,
              (absl::Mutex & key_mutex, std::string_view key,
               const absl::flat_hash_set<std::string_view>& value_set),
              (override));
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_CACHE_MOCKS_H_
