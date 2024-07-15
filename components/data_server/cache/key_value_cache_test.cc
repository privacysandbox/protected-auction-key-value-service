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

#include "components/data_server/cache/key_value_cache.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/notification.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/get_key_value_set_result.h"
#include "components/data_server/cache/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/base_types.pb.h"
#include "src/telemetry/telemetry_provider.h"

namespace kv_server {

class KeyValueCacheTestPeer {
 public:
  KeyValueCacheTestPeer() = delete;
  static std::multimap<int64_t, std::string> ReadDeletedNodes(
      const KeyValueCache& c, std::string_view prefix = "") {
    absl::MutexLock lock(&c.mutex_);
    auto map_itr = c.deleted_nodes_map_.find(prefix);
    return map_itr == c.deleted_nodes_map_.end()
               ? std::multimap<int64_t, std::string>()
               : c.deleted_nodes_map_.find(prefix)->second;
  }
  static absl::flat_hash_map<std::string, kv_server::KeyValueCache::CacheValue>&
  ReadNodes(KeyValueCache& c) {
    absl::MutexLock lock(&c.mutex_);
    return c.map_;
  }

  static int GetDeletedSetNodesMapSize(const KeyValueCache& c,
                                       std::string prefix = "") {
    absl::MutexLock lock(&c.set_map_mutex_);
    auto map_itr = c.deleted_set_nodes_map_.find(prefix);
    return map_itr == c.deleted_set_nodes_map_.end() ? 0
                                                     : map_itr->second.size();
  }

  static absl::flat_hash_set<std::string> ReadDeletedSetNodesForTimestamp(
      const KeyValueCache& c, int64_t logical_commit_time, std::string_view key,
      std::string_view prefix = "") {
    absl::MutexLock lock(&c.set_map_mutex_);
    auto map_itr = c.deleted_set_nodes_map_.find(prefix);
    return map_itr == c.deleted_set_nodes_map_.end()
               ? absl::flat_hash_set<std::string>()
               : map_itr->second.find(logical_commit_time)
                     ->second.find(key)
                     ->second;
  }

  static int GetCacheKeyValueSetMapSize(KeyValueCache& c) {
    absl::MutexLock lock(&c.set_map_mutex_);
    return c.key_to_value_set_map_.size();
  }

  static KeyValueCache::SetValueMeta GetSetValueMeta(const KeyValueCache& c,
                                                     std::string_view key,
                                                     std::string_view value) {
    absl::MutexLock lock(&c.set_map_mutex_);
    auto iter = c.key_to_value_set_map_.find(key);
    return iter->second->second.find(value)->second;
  }
  static int GetSetValueSize(const KeyValueCache& c, std::string_view key) {
    absl::MutexLock lock(&c.set_map_mutex_);
    auto iter = c.key_to_value_set_map_.find(key);
    return iter->second->second.size();
  }

  static void CallCacheCleanup(
      privacy_sandbox::server_common::log::SafePathContext& log_context,
      KeyValueCache& c, int64_t logical_commit_time) {
    c.RemoveDeletedKeys(log_context, logical_commit_time);
  }
};

namespace {

using privacy_sandbox::server_common::TelemetryProvider;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

class SafePathTestLogContext
    : public privacy_sandbox::server_common::log::SafePathContext {
 public:
  SafePathTestLogContext() = default;
};

class CacheTest : public ::testing::Test {
 protected:
  CacheTest() {
    InitMetricsContextMap();
    request_context_ = std::make_shared<RequestContext>();
  }
  const RequestContext& GetRequestContext() { return *request_context_; }
  std::shared_ptr<RequestContext> request_context_;
  SafePathTestLogContext safe_path_log_context_;
};

TEST_F(CacheTest, RetrievesMatchingEntry) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_value", 1);
  absl::flat_hash_set<std::string_view> keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), keys);
  absl::flat_hash_set<std::string_view> wrong_keys = {"wrong_key"};
  EXPECT_FALSE(cache->GetKeyValuePairs(GetRequestContext(), keys).empty());
  EXPECT_TRUE(cache->GetKeyValuePairs(GetRequestContext(), wrong_keys).empty());
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
}

TEST_F(CacheTest, GetWithMultipleKeysReturnsMatchingValues) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue(safe_path_log_context_, "key1", "value1", 1);
  cache->UpdateKeyValue(safe_path_log_context_, "key2", "value2", 2);
  cache->UpdateKeyValue(safe_path_log_context_, "key3", "value3", 3);

  absl::flat_hash_set<std::string_view> full_keys = {"key1", "key2"};

  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), full_keys);
  EXPECT_EQ(kv_pairs.size(), 2);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("key1", "value1"),
                                             KVPairEq("key2", "value2")));
}

TEST_F(CacheTest, GetAfterUpdateReturnsNewValue) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_value", 1);

  absl::flat_hash_set<std::string_view> keys = {"my_key"};

  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), keys);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));

  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_new_value", 2);

  kv_pairs = cache->GetKeyValuePairs(GetRequestContext(), keys);
  EXPECT_EQ(kv_pairs.size(), 1);
  EXPECT_THAT(kv_pairs,
              UnorderedElementsAre(KVPairEq("my_key", "my_new_value")));
}

TEST_F(CacheTest, GetAfterUpdateDifferentKeyReturnsSameValue) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_value", 1);
  cache->UpdateKeyValue(safe_path_log_context_, "new_key", "new_value", 2);

  absl::flat_hash_set<std::string_view> keys = {"my_key"};

  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), keys);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
}

TEST_F(CacheTest, GetForEmptyCacheReturnsEmptyList) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  absl::flat_hash_set<std::string_view> keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), keys);
  EXPECT_EQ(kv_pairs.size(), 0);
}

TEST_F(CacheTest, GetForCacheReturnsValueSet) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  std::vector<std::string_view> values = {"v1", "v2"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet(GetRequestContext(), {"my_key"})
          ->GetValueSet("my_key");
  EXPECT_THAT(value_set, UnorderedElementsAre("v1", "v2"));
}

TEST_F(CacheTest, GetForCacheMissingKeyReturnsEmptySet) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  std::vector<std::string_view> values = {"v1", "v2"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  auto get_key_value_set_result =
      cache->GetKeyValueSet(GetRequestContext(), {"missing_key", "my_key"});
  EXPECT_EQ(get_key_value_set_result->GetValueSet("missing_key").size(), 0);
  EXPECT_THAT(get_key_value_set_result->GetValueSet("my_key"),
              UnorderedElementsAre("v1", "v2"));
}

TEST_F(CacheTest, DeleteKeyTestRemovesKeyEntry) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_value", 1);
  cache->DeleteKey(safe_path_log_context_, "my_key", 2);
  absl::flat_hash_set<std::string_view> full_keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), full_keys);
  EXPECT_EQ(kv_pairs.size(), 0);
}

TEST_F(CacheTest, DeleteKeyValueSetWrongkeyDoesNotRemoveEntry) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_value", 1);
  cache->DeleteKey(safe_path_log_context_, "wrong_key", 1);
  absl::flat_hash_set<std::string_view> keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), keys);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
}

TEST_F(CacheTest, DeleteKeyValueSetRemovesValueEntry) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1", "v2", "v3"};
  std::vector<std::string_view> values_to_delete = {"v1", "v2"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values_to_delete), 2);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet(GetRequestContext(), {"my_key"})
          ->GetValueSet("my_key");
  EXPECT_THAT(value_set, UnorderedElementsAre("v3"));
  auto value_meta_v3 =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v3");
  EXPECT_EQ(value_meta_v3.last_logical_commit_time, 1);
  EXPECT_EQ(value_meta_v3.is_deleted, false);

  auto value_meta_v1_deleted =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta_v1_deleted.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta_v1_deleted.is_deleted, true);

  auto value_meta_v2_deleted =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v2");
  EXPECT_EQ(value_meta_v2_deleted.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta_v2_deleted.is_deleted, true);
}

TEST_F(CacheTest, DeleteKeyValueSetWrongKeyDoesNotRemoveKeyValueEntry) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1", "v2", "v3"};
  std::vector<std::string_view> values_to_delete = {"v1"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet(safe_path_log_context_, "wrong_key",
                           absl::Span<std::string_view>(values_to_delete), 2);
  std::unique_ptr<GetKeyValueSetResult> result =
      cache->GetKeyValueSet(GetRequestContext(), {"my_key", "wrong_key"});
  EXPECT_THAT(result->GetValueSet("my_key"),
              UnorderedElementsAre("v1", "v2", "v3"));
  EXPECT_EQ(result->GetValueSet("wrong_key").size(), 0);
  // Reset the unique pointer to destroy the GetKeyValueSetResult object which
  // holds the key locks in the cache map. This reset is required before calling
  // the GetSetValueMeta to avoid potential deadlocks,
  // as the "GetSetValueMeta" call will need to acquire the
  // cache map lock in the same thread.
  result.reset();

  auto value_meta_v1 =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta_v1.last_logical_commit_time, 1);
  EXPECT_EQ(value_meta_v1.is_deleted, false);

  auto value_meta_v1_deleted_for_wrong_key =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "wrong_key", "v1");
  EXPECT_EQ(value_meta_v1_deleted_for_wrong_key.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta_v1_deleted_for_wrong_key.is_deleted, true);
}

TEST_F(CacheTest, DeleteKeyValueSetWrongValueDoesNotRemoveEntry) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1", "v2", "v3"};
  std::vector<std::string_view> values_to_delete = {"v4"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values_to_delete), 2);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet(GetRequestContext(), {"my_key"})
          ->GetValueSet("my_key");
  EXPECT_THAT(value_set, UnorderedElementsAre("v1", "v2", "v3"));
  auto value_meta_v1 =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta_v1.last_logical_commit_time, 1);
  EXPECT_EQ(value_meta_v1.is_deleted, false);

  auto value_meta_v4_deleted =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v4");
  EXPECT_EQ(value_meta_v4_deleted.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta_v4_deleted.is_deleted, true);

  int value_set_in_cache_size =
      KeyValueCacheTestPeer::GetSetValueSize(*cache, "my_key");
  EXPECT_EQ(value_set_in_cache_size, 4);
}

TEST_F(CacheTest, OutOfOrderUpdateAfterUpdateWorks) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_value", 2);

  absl::flat_hash_set<std::string_view> keys = {"my_key"};

  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), keys);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));

  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_new_value", 1);

  kv_pairs = cache->GetKeyValuePairs(GetRequestContext(), keys);
  EXPECT_EQ(kv_pairs.size(), 1);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
}

TEST_F(CacheTest, DeleteKeyOutOfOrderDeleteAfterUpdateWorks) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->DeleteKey(safe_path_log_context_, "my_key", 2);
  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_value", 1);
  absl::flat_hash_set<std::string_view> full_keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), full_keys);
  EXPECT_EQ(kv_pairs.size(), 0);
}

TEST_F(CacheTest, DeleteKeyOutOfOrderUpdateAfterDeleteWorks) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_value", 2);
  cache->DeleteKey(safe_path_log_context_, "my_key", 1);
  absl::flat_hash_set<std::string_view> full_keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), full_keys);
  EXPECT_EQ(kv_pairs.size(), 1);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
}

TEST_F(CacheTest, DeleteKeyInOrderUpdateAfterDeleteWorks) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->DeleteKey(safe_path_log_context_, "my_key", 1);
  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_value", 2);
  absl::flat_hash_set<std::string_view> full_keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), full_keys);
  EXPECT_EQ(kv_pairs.size(), 1);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
}

TEST_F(CacheTest, DeleteKeyInOrderDeleteAfterUpdateWorks) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_value", 1);
  cache->DeleteKey(safe_path_log_context_, "my_key", 2);
  absl::flat_hash_set<std::string_view> full_keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), full_keys);
  EXPECT_EQ(kv_pairs.size(), 0);
}

TEST_F(CacheTest, UpdateSetTestUpdateAfterUpdateWithSameValue) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 2);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet(GetRequestContext(), {"my_key"})
          ->GetValueSet("my_key");
  EXPECT_THAT(value_set, UnorderedElementsAre("v1"));
  auto value_meta =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta.is_deleted, false);
}

TEST_F(CacheTest, UpdateSetTestUpdateAfterUpdateWithDifferentValue) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> first_value = {"v1"};
  std::vector<std::string_view> second_value = {"v2"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(first_value), 1);
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(second_value), 2);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet(GetRequestContext(), {"my_key"})
          ->GetValueSet("my_key");
  EXPECT_THAT(value_set, UnorderedElementsAre("v1", "v2"));
  auto value_meta_v1 =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta_v1.last_logical_commit_time, 1);
  EXPECT_EQ(value_meta_v1.is_deleted, false);
  auto value_meta_v2 =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v2");
  EXPECT_EQ(value_meta_v2.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta_v2.is_deleted, false);
}

TEST_F(CacheTest, InOrderUpdateSetInsertAfterDeleteExpectInsert) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1"};
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 2);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet(GetRequestContext(), {"my_key"})
          ->GetValueSet("my_key");
  EXPECT_THAT(value_set, UnorderedElementsAre("v1"));
  auto value_meta =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta.is_deleted, false);
}

TEST_F(CacheTest, InOrderUpdateSetDeleteAfterInsert) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 2);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet(GetRequestContext(), {"my_key"})
          ->GetValueSet("my_key");
  EXPECT_EQ(value_set.size(), 0);
  auto value_meta_v1 =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta_v1.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta_v1.is_deleted, true);
}

TEST_F(CacheTest, OutOfOrderUpdateSetInsertAfterDeleteExpectNoInsert) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1"};
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 2);
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet(GetRequestContext(), {"my_key"})
          ->GetValueSet("my_key");
  EXPECT_EQ(value_set.size(), 0);
  auto value_meta =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta.is_deleted, true);
}

TEST_F(CacheTest, OutOfOrderUpdateSetDeleteAfterInsertExpectNoDelete) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 2);
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet(GetRequestContext(), {"my_key"})
          ->GetValueSet("my_key");
  EXPECT_THAT(value_set, UnorderedElementsAre("v1"));
  auto value_meta_v1 =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta_v1.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta_v1.is_deleted, false);
}

TEST_F(CacheTest, CleanupTimestampsInsertAKeyDoesntUpdateDeletedNodes) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_value", 1);

  auto deleted_nodes = KeyValueCacheTestPeer::ReadDeletedNodes(*cache);
  EXPECT_EQ(deleted_nodes.size(), 0);
}

TEST_F(CacheTest, CleanupTimestampsRemoveDeletedKeysRemovesOldRecords) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_value", 1);
  cache->DeleteKey(safe_path_log_context_, "my_key", 2);

  cache->RemoveDeletedKeys(safe_path_log_context_, 3);

  auto deleted_nodes = KeyValueCacheTestPeer::ReadDeletedNodes(*cache);
  EXPECT_EQ(deleted_nodes.size(), 0);

  auto& nodes = KeyValueCacheTestPeer::ReadNodes(*cache);
  EXPECT_EQ(nodes.size(), 0);
}

TEST_F(CacheTest, CleanupTimestampsRemoveDeletedKeysDoesntAffectNewRecords) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  cache->UpdateKeyValue(safe_path_log_context_, "my_key", "my_value", 5);
  cache->DeleteKey(safe_path_log_context_, "my_key", 6);

  cache->RemoveDeletedKeys(safe_path_log_context_, 2);

  auto deleted_nodes = KeyValueCacheTestPeer::ReadDeletedNodes(*cache);
  EXPECT_EQ(deleted_nodes.size(), 1);
  auto range = deleted_nodes.equal_range(6);
  ASSERT_NE(range.first, range.second);
  EXPECT_EQ(range.first->second, "my_key");
}

TEST_F(CacheTest,
       CleanupRemoveDeletedKeysRemovesOldRecordsDoesntAffectNewRecords) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  cache->UpdateKeyValue(safe_path_log_context_, "my_key1", "my_value", 1);
  cache->UpdateKeyValue(safe_path_log_context_, "my_key2", "my_value", 2);
  cache->UpdateKeyValue(safe_path_log_context_, "my_key3", "my_value", 3);
  cache->UpdateKeyValue(safe_path_log_context_, "my_key4", "my_value", 4);
  cache->UpdateKeyValue(safe_path_log_context_, "my_key5", "my_value", 5);

  cache->DeleteKey(safe_path_log_context_, "my_key3", 8);
  cache->DeleteKey(safe_path_log_context_, "key_tombstone", 8);
  cache->DeleteKey(safe_path_log_context_, "my_key1", 6);
  cache->DeleteKey(safe_path_log_context_, "my_key2", 7);

  cache->RemoveDeletedKeys(safe_path_log_context_, 7);

  auto deleted_nodes = KeyValueCacheTestPeer::ReadDeletedNodes(*cache);
  EXPECT_EQ(deleted_nodes.size(), 2);
  auto range = deleted_nodes.equal_range(8);
  std::vector<std::string> deleted_values;
  for (auto it = range.first; it != range.second; ++it) {
    deleted_values.push_back(it->second);
  }
  EXPECT_THAT(deleted_values, UnorderedElementsAre("key_tombstone", "my_key3"));

  absl::flat_hash_set<std::string_view> full_keys = {
      "my_key1", "my_key2", "my_key3", "my_key4", "my_key5",
  };
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), full_keys);
  EXPECT_EQ(kv_pairs.size(), 2);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key4", "my_value"),
                                             KVPairEq("my_key5", "my_value")));
}

TEST_F(CacheTest, CleanupTimestampsCantInsertOldRecordsAfterCleanup) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  cache->UpdateKeyValue(safe_path_log_context_, "my_key1", "my_value", 10);
  cache->DeleteKey(safe_path_log_context_, "my_key1", 12);
  cache->RemoveDeletedKeys(safe_path_log_context_, 13);

  auto deleted_nodes = KeyValueCacheTestPeer::ReadDeletedNodes(*cache);
  EXPECT_EQ(deleted_nodes.size(), 0);

  cache->UpdateKeyValue(safe_path_log_context_, "my_key1", "my_value", 10);

  absl::flat_hash_set<std::string_view> keys = {"my_key1"};

  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(), keys);
  EXPECT_EQ(kv_pairs.size(), 0);
}

TEST_F(CacheTest, CleanupTimestampsInsertKeyValueSetDoesntUpdateDeletedNodes) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"my_value"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 0);
}

TEST_F(CacheTest, CleanupTimestampsDeleteKeyValueSetExpectUpdateDeletedNodes) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"my_value"};
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet(safe_path_log_context_, "another_key",
                           absl::Span<std::string_view>(values), 1);
  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 1);
  EXPECT_EQ(KeyValueCacheTestPeer::ReadDeletedSetNodesForTimestamp(*cache, 1,
                                                                   "my_key")
                .size(),
            1);
  EXPECT_EQ(KeyValueCacheTestPeer::ReadDeletedSetNodesForTimestamp(
                *cache, 1, "another_key")
                .size(),
            1);
}

TEST_F(CacheTest, CleanupTimestampsRemoveDeletedKeyValuesRemovesOldRecords) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"my_value"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 2);
  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 1);

  cache->RemoveDeletedKeys(safe_path_log_context_, 3);
  deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 0);
  EXPECT_EQ(KeyValueCacheTestPeer::GetCacheKeyValueSetMapSize(*cache), 0);
}

TEST_F(CacheTest,
       CleanupTimestampsRemoveDeletedKeyValuesDoesntAffectNewRecords) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"my_value"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 5);
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 6);

  cache->RemoveDeletedKeys(safe_path_log_context_, 2);

  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 1);
  EXPECT_EQ(KeyValueCacheTestPeer::ReadDeletedSetNodesForTimestamp(*cache, 6,
                                                                   "my_key")
                .size(),
            1);
}

TEST_F(
    CacheTest,
    CleanupSetCacheRemoveDeletedKeysRemovesOldRecordsDoesntAffectNewRecords) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1", "v2"};
  std::vector<std::string_view> values_to_delete = {"v1"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key1",
                           absl::Span<std::string_view>(values), 1);
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key2",
                           absl::Span<std::string_view>(values), 2);
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key3",
                           absl::Span<std::string_view>(values), 3);
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key4",
                           absl::Span<std::string_view>(values), 4);

  cache->DeleteValuesInSet(safe_path_log_context_, "my_key3",
                           absl::Span<std::string_view>(values_to_delete), 4);
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key1",
                           absl::Span<std::string_view>(values_to_delete), 5);
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key2",
                           absl::Span<std::string_view>(values_to_delete), 6);

  cache->RemoveDeletedKeys(safe_path_log_context_, 5);

  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 1);
  EXPECT_EQ(KeyValueCacheTestPeer::ReadDeletedSetNodesForTimestamp(*cache, 6,
                                                                   "my_key2")
                .size(),
            1);
  auto get_value_set_result = cache->GetKeyValueSet(
      GetRequestContext(), {"my_key1", "my_key4", "my_key3"});
  EXPECT_THAT(get_value_set_result->GetValueSet("my_key4"),
              UnorderedElementsAre("v1", "v2"));
  EXPECT_THAT(get_value_set_result->GetValueSet("my_key3"),
              UnorderedElementsAre("v2"));
  EXPECT_THAT(get_value_set_result->GetValueSet("my_key1"),
              UnorderedElementsAre("v2"));
}

TEST_F(CacheTest, CleanupTimestampsSetCacheCantInsertOldRecordsAfterCleanup) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"my_value"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 2);
  cache->RemoveDeletedKeys(safe_path_log_context_, 3);

  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 0);
  EXPECT_EQ(KeyValueCacheTestPeer::GetCacheKeyValueSetMapSize(*cache), 0);

  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 2);

  absl::flat_hash_set<std::string_view> kv_set =
      cache->GetKeyValueSet(GetRequestContext(), {"my_key"})
          ->GetValueSet("my_key");
  EXPECT_EQ(kv_set.size(), 0);
}

TEST_F(CacheTest, CleanupTimestampsCantAddOldDeletedRecordsAfterCleanup) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"my_value"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 2);
  cache->RemoveDeletedKeys(safe_path_log_context_, 3);

  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 0);
  EXPECT_EQ(KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache), 0);

  // Old delete
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 2);
  deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 0);
  EXPECT_EQ(KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache), 0);

  // New delete
  cache->DeleteValuesInSet(safe_path_log_context_, "my_key",
                           absl::Span<std::string_view>(values), 4);
  deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 1);
  EXPECT_EQ(KeyValueCacheTestPeer::GetCacheKeyValueSetMapSize(*cache), 1);
  auto value_meta =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "my_value");
  EXPECT_EQ(value_meta.is_deleted, true);
  EXPECT_EQ(value_meta.last_logical_commit_time, 4);

  absl::flat_hash_set<std::string_view> kv_set =
      cache->GetKeyValueSet(GetRequestContext(), {"my_key"})
          ->GetValueSet("my_key");
  EXPECT_EQ(kv_set.size(), 0);
}

TEST_F(CacheTest, ConcurrentGetAndGet) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys_lookup_request = {"key1", "key2"};
  std::vector<std::string_view> values_for_key1 = {"v1"};
  std::vector<std::string_view> values_for_key2 = {"v2"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "key1",
                           absl::Span<std::string_view>(values_for_key1), 1);
  cache->UpdateKeyValueSet(safe_path_log_context_, "key2",
                           absl::Span<std::string_view>(values_for_key2), 1);
  absl::Notification start;
  auto& request_context = GetRequestContext();
  auto lookup_fn = [&cache, &keys_lookup_request, &start, &request_context]() {
    start.WaitForNotification();
    auto result = cache->GetKeyValueSet(request_context, keys_lookup_request);
    EXPECT_THAT(result->GetValueSet("key1"), UnorderedElementsAre("v1"));
    EXPECT_THAT(result->GetValueSet("key2"), UnorderedElementsAre("v2"));
  };
  std::vector<std::thread> threads;
  for (int i = 0; i < std::min(20, (int)std::thread::hardware_concurrency());
       ++i) {
    threads.emplace_back(lookup_fn);
  }
  start.Notify();
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(CacheTest, ConcurrentGetAndUpdateExpectNoUpdate) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1"};
  std::vector<std::string_view> existing_values = {"v1"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "key1",
                           absl::Span<std::string_view>(existing_values), 3);
  absl::Notification start;
  auto& request_context = GetRequestContext();
  auto lookup_fn = [&cache, &keys, &start, &request_context]() {
    start.WaitForNotification();
    EXPECT_THAT(
        cache->GetKeyValueSet(request_context, keys)->GetValueSet("key1"),
        UnorderedElementsAre("v1"));
  };
  std::vector<std::string_view> new_values = {"v1"};
  auto update_fn = [&cache, &new_values, &start, this]() {
    start.WaitForNotification();
    cache->UpdateKeyValueSet(safe_path_log_context_, "key1",
                             absl::Span<std::string_view>(new_values), 1);
  };
  std::vector<std::thread> threads;
  for (int i = 0; i < std::min(20, (int)std::thread::hardware_concurrency());
       i++) {
    threads.emplace_back(lookup_fn);
    threads.emplace_back(update_fn);
  }
  start.Notify();
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(CacheTest, ConcurrentGetAndUpdateExpectUpdate) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1", "key2"};
  std::vector<std::string_view> existing_values = {"v1"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "key1",
                           absl::Span<std::string_view>(existing_values), 1);
  absl::Notification start;
  auto& request_context = GetRequestContext();
  auto lookup_fn = [&cache, &keys, &start, &request_context]() {
    start.WaitForNotification();
    EXPECT_THAT(
        cache->GetKeyValueSet(request_context, keys)->GetValueSet("key1"),
        UnorderedElementsAre("v1"));
  };
  std::vector<std::string_view> new_values_for_key2 = {"v2"};
  auto update_fn = [&cache, &new_values_for_key2, &start, this]() {
    // expect new value is inserted for key2
    start.WaitForNotification();
    cache->UpdateKeyValueSet(safe_path_log_context_, "key2",
                             absl::Span<std::string_view>(new_values_for_key2),
                             2);
  };
  std::vector<std::thread> threads;
  for (int i = 0; i < std::min(20, (int)std::thread::hardware_concurrency());
       i++) {
    threads.emplace_back(lookup_fn);
    threads.emplace_back(update_fn);
  }
  start.Notify();
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(CacheTest, ConcurrentGetAndDeleteExpectNoDelete) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1"};
  std::vector<std::string_view> existing_values = {"v1"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "key1",
                           absl::Span<std::string_view>(existing_values), 3);
  absl::Notification start;
  auto& request_context = GetRequestContext();
  auto lookup_fn = [&cache, &keys, &start, &request_context]() {
    start.WaitForNotification();
    EXPECT_THAT(
        cache->GetKeyValueSet(request_context, keys)->GetValueSet("key1"),
        UnorderedElementsAre("v1"));
  };
  std::vector<std::string_view> delete_values = {"v1"};
  auto delete_fn = [&cache, &delete_values, &start, this]() {
    // expect no delete
    start.WaitForNotification();
    cache->DeleteValuesInSet(safe_path_log_context_, "key1",
                             absl::Span<std::string_view>(delete_values), 1);
  };
  std::vector<std::thread> threads;
  for (int i = 0; i < std::min(20, (int)std::thread::hardware_concurrency());
       i++) {
    threads.emplace_back(lookup_fn);
    threads.emplace_back(delete_fn);
  }
  start.Notify();
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(CacheTest, ConcurrentGetAndCleanUp) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1", "key2"};
  std::vector<std::string_view> existing_values = {"v1"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "key1",
                           absl::Span<std::string_view>(existing_values), 3);
  cache->UpdateKeyValueSet(safe_path_log_context_, "key2",
                           absl::Span<std::string_view>(existing_values), 1);
  cache->DeleteValuesInSet(safe_path_log_context_, "key2",
                           absl::Span<std::string_view>(existing_values), 2);
  absl::Notification start;
  auto& request_context = GetRequestContext();
  auto lookup_fn = [&cache, &keys, &start, &request_context]() {
    start.WaitForNotification();
    EXPECT_THAT(
        cache->GetKeyValueSet(request_context, keys)->GetValueSet("key1"),
        UnorderedElementsAre("v1"));
    EXPECT_EQ(cache->GetKeyValueSet(request_context, keys)
                  ->GetValueSet("key2")
                  .size(),
              0);
  };
  auto cleanup_fn = [&cache, &start, this]() {
    // clean up old records
    start.WaitForNotification();
    KeyValueCacheTestPeer::CallCacheCleanup(safe_path_log_context_, *cache, 3);
  };
  std::vector<std::thread> threads;
  for (int i = 0; i < std::min(20, (int)std::thread::hardware_concurrency());
       i++) {
    threads.emplace_back(lookup_fn);
    threads.emplace_back(cleanup_fn);
  }
  start.Notify();
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(CacheTest, ConcurrentUpdateAndUpdateExpectUpdateBoth) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1", "key2"};
  std::vector<std::string_view> values_for_key1 = {"v1"};
  absl::Notification start;
  auto& request_context = GetRequestContext();
  auto update_key1 = [&cache, &keys, &values_for_key1, &start, &request_context,
                      this]() {
    start.WaitForNotification();
    // expect new value is inserted for key1
    cache->UpdateKeyValueSet(safe_path_log_context_, "key1",
                             absl::Span<std::string_view>(values_for_key1), 1);
    EXPECT_THAT(
        cache->GetKeyValueSet(request_context, keys)->GetValueSet("key1"),
        UnorderedElementsAre("v1"));
  };
  std::vector<std::string_view> values_for_key2 = {"v2"};
  auto update_key2 = [&cache, &keys, &values_for_key2, &start, &request_context,
                      this]() {
    // expect new value is inserted for key2
    start.WaitForNotification();
    cache->UpdateKeyValueSet(safe_path_log_context_, "key2",
                             absl::Span<std::string_view>(values_for_key2), 2);
    EXPECT_THAT(
        cache->GetKeyValueSet(request_context, keys)->GetValueSet("key2"),
        UnorderedElementsAre("v2"));
  };
  std::vector<std::thread> threads;
  for (int i = 0; i < std::min(20, (int)std::thread::hardware_concurrency());
       i++) {
    threads.emplace_back(update_key1);
    threads.emplace_back(update_key2);
  }
  start.Notify();
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(CacheTest, ConcurrentUpdateAndDelete) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1", "key2"};
  std::vector<std::string_view> values_for_key1 = {"v1"};
  absl::Notification start;
  auto& request_context = GetRequestContext();
  auto update_key1 = [&cache, &keys, &values_for_key1, &start, &request_context,
                      this]() {
    start.WaitForNotification();
    // expect new value is inserted for key1
    cache->UpdateKeyValueSet(safe_path_log_context_, "key1",
                             absl::Span<std::string_view>(values_for_key1), 1);
    EXPECT_THAT(
        cache->GetKeyValueSet(request_context, keys)->GetValueSet("key1"),
        UnorderedElementsAre("v1"));
  };
  // Update existing value for key2
  std::vector<std::string_view> existing_values_for_key2 = {"v1", "v2"};
  cache->UpdateKeyValueSet(
      safe_path_log_context_, "key2",
      absl::Span<std::string_view>(existing_values_for_key2), 1);
  std::vector<std::string_view> values_to_delete_for_key2 = {"v1"};

  auto delete_key2 = [&cache, &keys, &values_to_delete_for_key2, &start,
                      &request_context, this]() {
    start.WaitForNotification();
    // expect value is deleted for key2
    cache->DeleteValuesInSet(
        safe_path_log_context_, "key2",
        absl::Span<std::string_view>(values_to_delete_for_key2), 2);
    EXPECT_THAT(
        cache->GetKeyValueSet(request_context, keys)->GetValueSet("key2"),
        UnorderedElementsAre("v2"));
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < std::min(20, (int)std::thread::hardware_concurrency());
       i++) {
    threads.emplace_back(update_key1);
    threads.emplace_back(delete_key2);
  }
  start.Notify();
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(CacheTest, ConcurrentUpdateAndCleanUp) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1"};
  std::vector<std::string_view> values_for_key1 = {"v1"};
  absl::Notification start;
  auto& request_context = GetRequestContext();
  auto update_fn = [&cache, &keys, &values_for_key1, &start, &request_context,
                    this]() {
    start.WaitForNotification();
    cache->UpdateKeyValueSet(safe_path_log_context_, "key1",
                             absl::Span<std::string_view>(values_for_key1), 2);
    EXPECT_THAT(
        cache->GetKeyValueSet(request_context, keys)->GetValueSet("key1"),
        UnorderedElementsAre("v1"));
  };
  auto cleanup_fn = [&cache, &start, this]() {
    start.WaitForNotification();
    KeyValueCacheTestPeer::CallCacheCleanup(safe_path_log_context_, *cache, 1);
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < std::min(20, (int)std::thread::hardware_concurrency());
       i++) {
    threads.emplace_back(update_fn);
    threads.emplace_back(cleanup_fn);
  }
  start.Notify();
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(CacheTest, ConcurrentDeleteAndCleanUp) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1"};
  std::vector<std::string_view> values_for_key1 = {"v1"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "key1",
                           absl::Span<std::string_view>(values_for_key1), 1);
  absl::Notification start;
  auto& request_context = GetRequestContext();
  auto delete_fn = [&cache, &keys, &values_for_key1, &start, &request_context,
                    this]() {
    start.WaitForNotification();
    // expect new value is deleted for key1
    cache->DeleteValuesInSet(safe_path_log_context_, "key1",
                             absl::Span<std::string_view>(values_for_key1), 2);
    EXPECT_EQ(cache->GetKeyValueSet(request_context, keys)
                  ->GetValueSet("key1")
                  .size(),
              0);
  };
  auto cleanup_fn = [&cache, &start, this]() {
    start.WaitForNotification();
    KeyValueCacheTestPeer::CallCacheCleanup(safe_path_log_context_, *cache, 1);
  };
  std::vector<std::thread> threads;
  for (int i = 0; i < std::min(20, (int)std::thread::hardware_concurrency());
       i++) {
    threads.emplace_back(delete_fn);
    threads.emplace_back(cleanup_fn);
  }
  start.Notify();
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(CacheTest, ConcurrentGetUpdateDeleteCleanUp) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1", "key2"};
  std::vector<std::string_view> existing_values_for_key1 = {"v1"};
  std::vector<std::string_view> existing_values_for_key2 = {"v1"};
  cache->UpdateKeyValueSet(
      safe_path_log_context_, "key1",
      absl::Span<std::string_view>(existing_values_for_key1), 1);
  cache->UpdateKeyValueSet(
      safe_path_log_context_, "key2",
      absl::Span<std::string_view>(existing_values_for_key2), 1);

  std::vector<std::string_view> values_to_insert_for_key2 = {"v2"};
  std::vector<std::string_view> values_to_delete_for_key2 = {"v1"};
  absl::Notification start;
  auto insert_for_key2 = [&cache, &values_to_insert_for_key2, &start, this]() {
    start.WaitForNotification();
    cache->UpdateKeyValueSet(
        safe_path_log_context_, "key2",
        absl::Span<std::string_view>(values_to_insert_for_key2), 2);
  };
  auto delete_for_key2 = [&cache, &values_to_delete_for_key2, &start, this]() {
    start.WaitForNotification();
    cache->DeleteValuesInSet(
        safe_path_log_context_, "key2",
        absl::Span<std::string_view>(values_to_delete_for_key2), 2);
  };
  auto cleanup = [&cache, &start, this]() {
    start.WaitForNotification();
    KeyValueCacheTestPeer::CallCacheCleanup(safe_path_log_context_, *cache, 1);
  };
  auto& request_context = GetRequestContext();
  auto lookup_for_key1 = [&cache, &keys, &start, &request_context]() {
    start.WaitForNotification();
    EXPECT_THAT(
        cache->GetKeyValueSet(request_context, keys)->GetValueSet("key1"),
        UnorderedElementsAre("v1"));
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < std::min(20, (int)std::thread::hardware_concurrency());
       i++) {
    threads.emplace_back(insert_for_key2);
    threads.emplace_back(cleanup);
    threads.emplace_back(delete_for_key2);
    threads.emplace_back(lookup_for_key1);
  }
  start.Notify();
  for (auto& thread : threads) {
    thread.join();
  }
  auto look_up_result_for_key2 =
      cache->GetKeyValueSet(request_context, keys)->GetValueSet("key2");
  EXPECT_THAT(look_up_result_for_key2, UnorderedElementsAre("v2"));
}

TEST_F(CacheTest, MultiplePrefixKeyValueUpdates) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  // Call remove deleted keys for prefix1 to update the max delete cutoff
  // timestamp
  cache->RemoveDeletedKeys(safe_path_log_context_, 1, "prefix1");
  cache->UpdateKeyValue(safe_path_log_context_, "prefix1-key", "value1", 2,
                        "prefix1");
  cache->UpdateKeyValue(safe_path_log_context_, "prefix2-key", "value2", 1,
                        "prefix2");
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(),
                              {"prefix1-key", "prefix2-key"});
  EXPECT_EQ(kv_pairs.size(), 2);
  EXPECT_EQ(kv_pairs["prefix1-key"], "value1");
  EXPECT_EQ(kv_pairs["prefix2-key"], "value2");
}

TEST_F(CacheTest, MultiplePrefixKeyValueNoUpdateForAnother) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  // Call remove deleted keys for prefix1 to update the max delete cutoff
  // timestamp
  cache->RemoveDeletedKeys(safe_path_log_context_, 2, "prefix1");
  // Expect no update for prefix1
  cache->UpdateKeyValue(safe_path_log_context_, "prefix1-key", "value1", 1,
                        "prefix1");
  cache->UpdateKeyValue(safe_path_log_context_, "prefix2-key", "value2", 1,
                        "prefix2");
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(),
                              {"prefix1-key", "prefix2-key"});
  EXPECT_EQ(kv_pairs.size(), 1);
  EXPECT_EQ(kv_pairs["prefix2-key"], "value2");
}

TEST_F(CacheTest, MultiplePrefixKeyValueNoDeleteForAnother) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  // Call remove deleted keys for prefix1 to update the max delete cutoff
  // timestamp
  cache->RemoveDeletedKeys(safe_path_log_context_, 2, "prefix1");
  cache->UpdateKeyValue(safe_path_log_context_, "prefix1-key", "value1", 3,
                        "prefix1");
  // Expect no deletion
  cache->DeleteKey(safe_path_log_context_, "prefix1-key", 1, "prefix1");
  cache->UpdateKeyValue(safe_path_log_context_, "prefix2-key", "value2", 1,
                        "prefix2");
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(),
                              {"prefix1-key", "prefix2-key"});
  EXPECT_EQ(kv_pairs.size(), 2);
  EXPECT_EQ(kv_pairs["prefix1-key"], "value1");
  EXPECT_EQ(kv_pairs["prefix2-key"], "value2");
}

TEST_F(CacheTest, MultiplePrefixKeyValueDeletesAndUpdates) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  cache->DeleteKey(safe_path_log_context_, "prefix1-key", 2, "prefix1");
  cache->UpdateKeyValue(safe_path_log_context_, "prefix1-key", "value1", 1,
                        "prefix1");
  cache->UpdateKeyValue(safe_path_log_context_, "prefix2-key", "value2", 1,
                        "prefix2");
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(),
                              {"prefix1-key", "prefix2-key"});
  EXPECT_EQ(kv_pairs.size(), 1);
  EXPECT_EQ(kv_pairs["prefix2-key"], "value2");
  auto deleted_nodes =
      KeyValueCacheTestPeer::ReadDeletedNodes(*cache, "prefix1");
  EXPECT_EQ(deleted_nodes.size(), 1);
  deleted_nodes = KeyValueCacheTestPeer::ReadDeletedNodes(*cache, "prefix2");
  EXPECT_EQ(deleted_nodes.size(), 0);
}

TEST_F(CacheTest, MultiplePrefixKeyValueUpdatesAndDeletes) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  cache->UpdateKeyValue(safe_path_log_context_, "prefix1-key", "value1", 2,
                        "prefix1");
  // Expects no deletes
  cache->DeleteKey(safe_path_log_context_, "prefix1-key", 1, "prefix1");
  cache->UpdateKeyValue(safe_path_log_context_, "prefix2-key", "value2", 1,
                        "prefix2");
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(GetRequestContext(),
                              {"prefix1-key", "prefix2-key"});
  EXPECT_EQ(kv_pairs.size(), 2);
  EXPECT_EQ(kv_pairs["prefix1-key"], "value1");
  EXPECT_EQ(kv_pairs["prefix2-key"], "value2");
  auto deleted_nodes = KeyValueCacheTestPeer::ReadDeletedNodes(*cache);
  EXPECT_EQ(deleted_nodes.size(), 0);
}

TEST_F(CacheTest, MultiplePrefixKeyValueSetUpdates) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values1 = {"v1", "v2"};
  std::vector<std::string_view> values2 = {"v3", "v4"};
  // Call remove deleted keys for prefix1 to update the max delete cutoff
  // timestamp
  cache->RemoveDeletedKeys(safe_path_log_context_, 1, "prefix1");
  cache->UpdateKeyValueSet(safe_path_log_context_, "prefix1-key",
                           absl::Span<std::string_view>(values1), 2, "prefix1");
  cache->UpdateKeyValueSet(safe_path_log_context_, "prefix2-key",
                           absl::Span<std::string_view>(values2), 1, "prefix2");

  auto get_value_set_result = cache->GetKeyValueSet(
      GetRequestContext(), {"prefix1-key", "prefix2-key"});
  EXPECT_THAT(get_value_set_result->GetValueSet("prefix1-key"),
              UnorderedElementsAre("v1", "v2"));
  EXPECT_THAT(get_value_set_result->GetValueSet("prefix2-key"),
              UnorderedElementsAre("v3", "v4"));
}

TEST_F(CacheTest, MultipleKeyValueSetNoUpdateForAnother) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values1 = {"v1", "v2"};
  std::vector<std::string_view> values2 = {"v3", "v4"};
  // Call remove deleted keys for prefix1 to update the max delete cutoff
  // timestamp
  cache->RemoveDeletedKeys(safe_path_log_context_, 2, "prefix1");
  cache->UpdateKeyValueSet(safe_path_log_context_, "prefix1-key",
                           absl::Span<std::string_view>(values1), 1, "prefix1");
  cache->UpdateKeyValueSet(safe_path_log_context_, "prefix2-key",
                           absl::Span<std::string_view>(values2), 1, "prefix2");
  auto get_value_set_result = cache->GetKeyValueSet(
      GetRequestContext(), {"prefix1-key", "prefix2-key"});
  EXPECT_EQ(get_value_set_result->GetValueSet("prefix1-key").size(), 0);
  EXPECT_THAT(get_value_set_result->GetValueSet("prefix2-key"),
              UnorderedElementsAre("v3", "v4"));
}

TEST_F(CacheTest, MultiplePrefixKeyValueSetDeletesAndUpdates) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values1 = {"v1", "v2"};
  std::vector<std::string_view> values_to_delete = {"v1"};
  std::vector<std::string_view> values2 = {"v3", "v4"};
  cache->DeleteValuesInSet(safe_path_log_context_, "prefix1-key",
                           absl::Span<std::string_view>(values_to_delete), 2,
                           "prefix1");
  cache->UpdateKeyValueSet(safe_path_log_context_, "prefix1-key",
                           absl::Span<std::string_view>(values1), 1, "prefix1");
  cache->UpdateKeyValueSet(safe_path_log_context_, "prefix2-key",
                           absl::Span<std::string_view>(values2), 1, "prefix2");
  auto get_value_set_result = cache->GetKeyValueSet(
      GetRequestContext(), {"prefix1-key", "prefix2-key"});
  EXPECT_THAT(get_value_set_result->GetValueSet("prefix1-key"),
              UnorderedElementsAre("v2"));
  EXPECT_THAT(get_value_set_result->GetValueSet("prefix2-key"),
              UnorderedElementsAre("v3", "v4"));
  EXPECT_EQ(KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache, "prefix1"),
            1);
  EXPECT_EQ(KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache, "prefix2"),
            0);
}

TEST_F(CacheTest, MultiplePrefixKeyValueSetUpdatesAndDeletes) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values1 = {"v1", "v2"};
  std::vector<std::string_view> values_to_delete = {"v1"};
  std::vector<std::string_view> values2 = {"v3", "v4"};

  cache->UpdateKeyValueSet(safe_path_log_context_, "prefix1-key",
                           absl::Span<std::string_view>(values1), 2, "prefix1");
  cache->UpdateKeyValueSet(safe_path_log_context_, "prefix2-key",
                           absl::Span<std::string_view>(values2), 1, "prefix2");
  // Expect no deletes
  cache->DeleteValuesInSet(safe_path_log_context_, "prefix1-key",
                           absl::Span<std::string_view>(values_to_delete), 1,
                           "prefix1");
  auto get_value_set_result = cache->GetKeyValueSet(
      GetRequestContext(), {"prefix1-key", "prefix2-key"});
  EXPECT_THAT(get_value_set_result->GetValueSet("prefix1-key"),
              UnorderedElementsAre("v1", "v2"));
  EXPECT_THAT(get_value_set_result->GetValueSet("prefix2-key"),
              UnorderedElementsAre("v3", "v4"));
  EXPECT_EQ(KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache, "prefix1"),
            0);
  EXPECT_EQ(KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache, "prefix2"),
            0);
}

TEST_F(CacheTest, MultiplePrefixTimestampKeyValueCleanUps) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  cache->UpdateKeyValue(safe_path_log_context_, "prefix1-key", "value", 2,
                        "prefix1");
  cache->DeleteKey(safe_path_log_context_, "prefix1-key", 3, "prefix1");
  cache->UpdateKeyValue(safe_path_log_context_, "prefix2-key", "value", 2,
                        "prefix2");
  cache->DeleteKey(safe_path_log_context_, "prefix2-key", 5, "prefix2");
  auto deleted_nodes_for_prefix1 =
      KeyValueCacheTestPeer::ReadDeletedNodes(*cache, "prefix1");
  EXPECT_EQ(deleted_nodes_for_prefix1.size(), 1);
  cache->RemoveDeletedKeys(safe_path_log_context_, 4, "prefix1");
  cache->RemoveDeletedKeys(safe_path_log_context_, 4, "prefix2");
  deleted_nodes_for_prefix1 =
      KeyValueCacheTestPeer::ReadDeletedNodes(*cache, "prefix1");
  EXPECT_EQ(deleted_nodes_for_prefix1.size(), 0);
  auto deleted_nodes_for_prefix2 =
      KeyValueCacheTestPeer::ReadDeletedNodes(*cache, "prefix2");
  EXPECT_EQ(deleted_nodes_for_prefix2.size(), 1);
}
TEST_F(CacheTest, MultiplePrefixTimestampKeyValueSetCleanUps) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1", "v2"};
  std::vector<std::string_view> values_to_delete = {"v1"};
  cache->UpdateKeyValueSet(safe_path_log_context_, "prefix1-key",
                           absl::Span<std::string_view>(values), 2, "prefix1");
  cache->UpdateKeyValueSet(safe_path_log_context_, "prefix2-key",
                           absl::Span<std::string_view>(values), 2, "prefix2");
  cache->DeleteValuesInSet(safe_path_log_context_, "prefix1-key",
                           absl::Span<std::string_view>(values_to_delete), 3,
                           "prefix1");
  cache->DeleteValuesInSet(safe_path_log_context_, "prefix2-key",
                           absl::Span<std::string_view>(values_to_delete), 5,
                           "prefix2");
  cache->RemoveDeletedKeys(safe_path_log_context_, 4, "prefix1");
  cache->RemoveDeletedKeys(safe_path_log_context_, 4, "prefix2");
  EXPECT_EQ(KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache, "prefix1"),
            0);
  EXPECT_EQ(KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache, "prefix2"),
            1);
}

TEST_F(CacheTest, VerifyUpdatingUInt32Sets) {
  auto cache = KeyValueCache::Create();
  auto& request_context = GetRequestContext();
  auto keys = absl::flat_hash_set<std::string_view>({"set1", "set2"});
  {
    auto result = cache->GetUInt32ValueSet(request_context, keys);
    for (const auto& key : keys) {
      auto* set = result->GetUInt32ValueSet(key);
      EXPECT_EQ(set, nullptr);
    }
  }
  {
    auto set1_values = std::vector<uint32_t>({1, 2, 3, 4, 5});
    cache->UpdateKeyValueSet(safe_path_log_context_, "set1",
                             absl::MakeSpan(set1_values), 1);
    auto result = cache->GetUInt32ValueSet(request_context, keys);
    auto* set = result->GetUInt32ValueSet("set1");
    ASSERT_TRUE(set != nullptr);
    EXPECT_THAT(set->GetValues(), UnorderedElementsAreArray(set1_values));
  }
  {
    auto set2_values = std::vector<uint32_t>({6, 7, 8, 9, 10});
    cache->UpdateKeyValueSet(safe_path_log_context_, "set2",
                             absl::MakeSpan(set2_values), 1);
    auto result = cache->GetUInt32ValueSet(request_context, keys);
    auto* set = result->GetUInt32ValueSet("set2");
    ASSERT_TRUE(set != nullptr);
    EXPECT_THAT(set->GetValues(), UnorderedElementsAreArray(set2_values));
  }
}

TEST_F(CacheTest, VerifyDeletingUInt32Sets) {
  auto cache = KeyValueCache::Create();
  auto& request_context = GetRequestContext();
  auto keys = absl::flat_hash_set<std::string_view>({"set1", "set2"});
  auto delete_values = std::vector<uint32_t>({1, 2, 6, 7});
  {
    auto set1_values = std::vector<uint32_t>({1, 2, 3, 4, 5});
    cache->UpdateKeyValueSet(safe_path_log_context_, "set1",
                             absl::MakeSpan(set1_values), 1);
    cache->DeleteValuesInSet(safe_path_log_context_, "set1",
                             absl::MakeSpan(delete_values), 2);
    auto result = cache->GetUInt32ValueSet(request_context, keys);
    auto* set = result->GetUInt32ValueSet("set1");
    ASSERT_TRUE(set != nullptr);
    EXPECT_THAT(set->GetValues(), UnorderedElementsAre(3, 4, 5));
  }
  {
    auto set2_values = std::vector<uint32_t>({6, 7, 8, 9, 10});
    cache->UpdateKeyValueSet(safe_path_log_context_, "set2",
                             absl::MakeSpan(set2_values), 1);
    cache->DeleteValuesInSet(safe_path_log_context_, "set2",
                             absl::MakeSpan(delete_values), 2);
    auto result = cache->GetUInt32ValueSet(request_context, keys);
    auto* set = result->GetUInt32ValueSet("set2");
    ASSERT_TRUE(set != nullptr);
    EXPECT_THAT(set->GetValues(), UnorderedElementsAre(8, 9, 10));
  }
}

TEST_F(CacheTest, VerifyCleaningUpUInt32Sets) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  auto& request_context = GetRequestContext();
  auto keys = absl::flat_hash_set<std::string_view>({"set1"});
  auto set1_values = std::vector<uint32_t>({1, 2, 3, 4, 5});
  auto delete_values = std::vector<uint32_t>({1, 2});
  {
    cache->UpdateKeyValueSet(safe_path_log_context_, "set1",
                             absl::MakeSpan(set1_values), 1);
    cache->DeleteValuesInSet(safe_path_log_context_, "set1",
                             absl::MakeSpan(delete_values), 2);
    auto result = cache->GetUInt32ValueSet(request_context, keys);
    auto* set = result->GetUInt32ValueSet("set1");
    ASSERT_TRUE(set != nullptr);
    EXPECT_THAT(set->GetValues(), UnorderedElementsAre(3, 4, 5));
    EXPECT_THAT(set->GetRemovedValues(),
                UnorderedElementsAreArray(delete_values));
  }
}

}  // namespace
}  // namespace kv_server
