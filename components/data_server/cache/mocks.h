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

#include "components/data_server/cache/cache.h"
#include "gmock/gmock.h"

namespace fledge::kv_server {

MATCHER_P2(CacheKeyEq, key, subkey, "") {
  return testing::ExplainMatchResult(
      testing::AllOf(testing::Field(&Cache::Key::subkey, subkey),
                     testing::Field(&Cache::Key::key, key)),
      arg, result_listener);
}

MATCHER_P(CacheKeyEq, cache_key, "") {
  return testing::ExplainMatchResult(
      testing::AllOf(testing::Field(&Cache::Key::subkey, cache_key.subkey),
                     testing::Field(&Cache::Key::key, cache_key.key)),
      arg, result_listener);
}

MATCHER_P2(KVPairEq, cache_key, value, "") {
  return testing::ExplainMatchResult(
      testing::Pair(CacheKeyEq(cache_key), value), arg, result_listener);
}

class MockCache : public Cache {
 public:
  MOCK_METHOD((std::vector<std::pair<Key, std::string>>), GetKeyValuePairs,
              (const std::vector<Key>& cache_key_list), (const, override));
  MOCK_METHOD(void, UpdateKeyValue, (Key full_key, std::string value),
              (override));
  MOCK_METHOD(void, DeleteKey, (Key full_key), (override));
};

class MockShardedCache : public ShardedCache {
 public:
  MOCK_METHOD(Cache&, GetMutableCacheShard, (KeyNamespace::Enum key_namespace),
              (override));
  MOCK_METHOD(const Cache&, GetCacheShard, (KeyNamespace::Enum key_namespace),
              (const, override));
};

}  // namespace fledge::kv_server

#endif  // COMPONENTS_DATA_SERVER_CACHE_MOCKS_H_
