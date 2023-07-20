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

namespace kv_server {

class KeyValueCacheTestPeer {
 public:
  KeyValueCacheTestPeer() = delete;
  static absl::btree_map<int64_t, std::string> ReadDeletedNodes(
      const KeyValueCache& c) {
    absl::MutexLock lock(&c.mutex_);
    return c.deleted_nodes_;
  }
  static absl::flat_hash_map<std::string, kv_server::KeyValueCache::CacheValue>&
  ReadNodes(KeyValueCache& c) {
    absl::MutexLock lock(&c.mutex_);
    return c.map_;
  }

  static int GetDeletedSetNodesMapSize(const KeyValueCache& c) {
    absl::MutexLock lock(&c.set_map_mutex_);
    return c.deleted_set_nodes_.size();
  }

  static absl::flat_hash_set<std::string> ReadDeletedSetNodesForTimestamp(
      const KeyValueCache& c, int64_t logical_commit_time,
      std::string_view key) {
    absl::MutexLock lock(&c.set_map_mutex_);
    return c.deleted_set_nodes_.find(logical_commit_time)
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

  static void CallCacheCleanup(KeyValueCache& c, int64_t logical_commit_time) {
    c.CleanUpKeyValueMap(logical_commit_time);
  }
};

namespace {

using testing::UnorderedElementsAre;

TEST(CacheTest, RetrievesMatchingEntry) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue("my_key", "my_value", 1);
  std::vector<std::string_view> keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(keys);
  std::vector<std::string_view> wrong_keys = {"wrong_key"};
  EXPECT_FALSE(cache->GetKeyValuePairs(keys).empty());
  EXPECT_TRUE(cache->GetKeyValuePairs(wrong_keys).empty());
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
}

TEST(CacheTest, GetWithMultipleKeysReturnsMatchingValues) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue("key1", "value1", 1);
  cache->UpdateKeyValue("key2", "value2", 2);
  cache->UpdateKeyValue("key3", "value3", 3);

  std::vector<std::string_view> full_keys = {"key1", "key2"};

  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(full_keys);
  EXPECT_EQ(kv_pairs.size(), 2);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("key1", "value1"),
                                             KVPairEq("key2", "value2")));
}

TEST(CacheTest, GetAfterUpdateReturnsNewValue) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue("my_key", "my_value", 1);

  std::vector<std::string_view> keys = {"my_key"};

  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(keys);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));

  cache->UpdateKeyValue("my_key", "my_new_value", 2);

  kv_pairs = cache->GetKeyValuePairs(keys);
  EXPECT_EQ(kv_pairs.size(), 1);
  EXPECT_THAT(kv_pairs,
              UnorderedElementsAre(KVPairEq("my_key", "my_new_value")));
}

TEST(CacheTest, GetAfterUpdateDifferentKeyReturnsSameValue) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue("my_key", "my_value", 1);
  cache->UpdateKeyValue("new_key", "new_value", 2);

  std::vector<std::string_view> keys = {"my_key"};

  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(keys);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
}

TEST(CacheTest, GetForEmptyCacheReturnsEmptyList) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  std::vector<std::string_view> keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(keys);
  EXPECT_EQ(kv_pairs.size(), 0);
}

TEST(CacheTest, GetForCacheReturnsValueSet) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  std::vector<std::string_view> values = {"v1", "v2"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 1);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet({"my_key"})->GetValueSet("my_key");
  EXPECT_THAT(value_set, UnorderedElementsAre("v1", "v2"));
}

TEST(CacheTest, GetForCacheMissingKeyReturnsEmptySet) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  std::vector<std::string_view> values = {"v1", "v2"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 1);
  auto get_key_value_set_result =
      cache->GetKeyValueSet({"missing_key", "my_key"});
  EXPECT_EQ(get_key_value_set_result->GetValueSet("missing_key").size(), 0);
  EXPECT_THAT(get_key_value_set_result->GetValueSet("my_key"),
              UnorderedElementsAre("v1", "v2"));
}

TEST(DeleteKeyTest, RemovesKeyEntry) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue("my_key", "my_value", 1);
  cache->DeleteKey("my_key", 2);
  std::vector<std::string_view> full_keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(full_keys);
  EXPECT_EQ(kv_pairs.size(), 0);
}

TEST(DeleteKeyValueSetTest, WrongkeyDoesNotRemoveEntry) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue("my_key", "my_value", 1);
  cache->DeleteKey("wrong_key", 1);
  std::vector<std::string_view> keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(keys);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
}

TEST(DeleteKeyValueSetTest, RemovesValueEntry) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1", "v2", "v3"};
  std::vector<std::string_view> values_to_delete = {"v1", "v2"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet("my_key",
                           absl::Span<std::string_view>(values_to_delete), 2);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet({"my_key"})->GetValueSet("my_key");
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

TEST(DeleteKeyValueSetTest, WrongKeyDoesNotRemoveKeyValueEntry) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1", "v2", "v3"};
  std::vector<std::string_view> values_to_delete = {"v1"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet("wrong_key",
                           absl::Span<std::string_view>(values_to_delete), 2);
  std::unique_ptr<GetKeyValueSetResult> result =
      cache->GetKeyValueSet({"my_key", "wrong_key"});
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

TEST(DeleteKeyValueSetTest, WrongValueDoesNotRemoveEntry) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1", "v2", "v3"};
  std::vector<std::string_view> values_to_delete = {"v4"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet("my_key",
                           absl::Span<std::string_view>(values_to_delete), 2);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet({"my_key"})->GetValueSet("my_key");
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

TEST(CacheTest, OutOfOrderUpdateAfterUpdateWorks) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue("my_key", "my_value", 2);

  std::vector<std::string_view> keys = {"my_key"};

  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(keys);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));

  cache->UpdateKeyValue("my_key", "my_new_value", 1);

  kv_pairs = cache->GetKeyValuePairs(keys);
  EXPECT_EQ(kv_pairs.size(), 1);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
}

TEST(DeleteKeyTest, OutOfOrderDeleteAfterUpdateWorks) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->DeleteKey("my_key", 2);
  cache->UpdateKeyValue("my_key", "my_value", 1);
  std::vector<std::string_view> full_keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(full_keys);
  EXPECT_EQ(kv_pairs.size(), 0);
}

TEST(DeleteKeyTest, OutOfOrderUpdateAfterDeleteWorks) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue("my_key", "my_value", 2);
  cache->DeleteKey("my_key", 1);
  std::vector<std::string_view> full_keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(full_keys);
  EXPECT_EQ(kv_pairs.size(), 1);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
}

TEST(DeleteKeyTest, InOrderUpdateAfterDeleteWorks) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->DeleteKey("my_key", 1);
  cache->UpdateKeyValue("my_key", "my_value", 2);
  std::vector<std::string_view> full_keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(full_keys);
  EXPECT_EQ(kv_pairs.size(), 1);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
}

TEST(DeleteKeyTest, InOrderDeleteAfterUpdateWorks) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue("my_key", "my_value", 1);
  cache->DeleteKey("my_key", 2);
  std::vector<std::string_view> full_keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(full_keys);
  EXPECT_EQ(kv_pairs.size(), 0);
}

TEST(UpateKeyValueSetTest, UpdateAfterUpdateWithSameValue) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 1);
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 2);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet({"my_key"})->GetValueSet("my_key");
  EXPECT_THAT(value_set, UnorderedElementsAre("v1"));
  auto value_meta =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta.is_deleted, false);
}

TEST(UpateKeyValueSetTest, UpdateAfterUpdateWithDifferentValue) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> first_value = {"v1"};
  std::vector<std::string_view> second_value = {"v2"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(first_value),
                           1);
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(second_value),
                           2);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet({"my_key"})->GetValueSet("my_key");
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

TEST(InOrderUpateKeyValueSetTest, InsertAfterDeleteExpectInsert) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1"};
  cache->DeleteValuesInSet("my_key", absl::Span<std::string_view>(values), 1);
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 2);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet({"my_key"})->GetValueSet("my_key");
  EXPECT_THAT(value_set, UnorderedElementsAre("v1"));
  auto value_meta =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta.is_deleted, false);
}

TEST(InOrderUpateKeyValueSetTest, DeleteAfterInsert) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet("my_key", absl::Span<std::string_view>(values), 2);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet({"my_key"})->GetValueSet("my_key");
  EXPECT_EQ(value_set.size(), 0);
  auto value_meta_v1 =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta_v1.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta_v1.is_deleted, true);
}

TEST(OutOfOrderUpateKeyValueSetTest, InsertAfterDeleteExpectNoInsert) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1"};
  cache->DeleteValuesInSet("my_key", absl::Span<std::string_view>(values), 2);
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 1);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet({"my_key"})->GetValueSet("my_key");
  EXPECT_EQ(value_set.size(), 0);
  auto value_meta =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta.is_deleted, true);
}

TEST(OutOfOrderUpateKeyValueSetTest, DeleteAfterInsertExpectNoDelete) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 2);
  cache->DeleteValuesInSet("my_key", absl::Span<std::string_view>(values), 1);
  absl::flat_hash_set<std::string_view> value_set =
      cache->GetKeyValueSet({"my_key"})->GetValueSet("my_key");
  EXPECT_THAT(value_set, UnorderedElementsAre("v1"));
  auto value_meta_v1 =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "v1");
  EXPECT_EQ(value_meta_v1.last_logical_commit_time, 2);
  EXPECT_EQ(value_meta_v1.is_deleted, false);
}

TEST(CleanUpTimestamps, InsertAKeyDoesntUpdateDeletedNodes) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  cache->UpdateKeyValue("my_key", "my_value", 1);

  auto deleted_nodes = KeyValueCacheTestPeer::ReadDeletedNodes(*cache);
  EXPECT_EQ(deleted_nodes.size(), 0);
}

TEST(CleanUpTimestamps, RemoveDeletedKeysRemovesOldRecords) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  cache->UpdateKeyValue("my_key", "my_value", 1);
  cache->DeleteKey("my_key", 2);

  cache->RemoveDeletedKeys(3);

  auto deleted_nodes = KeyValueCacheTestPeer::ReadDeletedNodes(*cache);
  EXPECT_EQ(deleted_nodes.size(), 0);

  auto& nodes = KeyValueCacheTestPeer::ReadNodes(*cache);
  EXPECT_EQ(nodes.size(), 0);
}

TEST(CleanUpTimestamps, RemoveDeletedKeysDoesntAffectNewRecords) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  cache->UpdateKeyValue("my_key", "my_value", 5);
  cache->DeleteKey("my_key", 6);

  cache->RemoveDeletedKeys(2);

  auto deleted_nodes = KeyValueCacheTestPeer::ReadDeletedNodes(*cache);
  EXPECT_EQ(deleted_nodes.size(), 1);
  EXPECT_EQ(deleted_nodes.at(6), "my_key");
}

TEST(CleanUpTimestamps,
     RemoveDeletedKeysRemovesOldRecordsDoesntAffectNewRecords) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  cache->UpdateKeyValue("my_key1", "my_value", 1);
  cache->UpdateKeyValue("my_key2", "my_value", 2);
  cache->UpdateKeyValue("my_key3", "my_value", 3);
  cache->UpdateKeyValue("my_key4", "my_value", 4);
  cache->UpdateKeyValue("my_key5", "my_value", 5);

  cache->DeleteKey("my_key3", 8);
  cache->DeleteKey("my_key1", 6);
  cache->DeleteKey("my_key2", 7);

  cache->RemoveDeletedKeys(7);

  auto deleted_nodes = KeyValueCacheTestPeer::ReadDeletedNodes(*cache);
  EXPECT_EQ(deleted_nodes.size(), 1);
  EXPECT_EQ(deleted_nodes.at(8), "my_key3");

  std::vector<std::string_view> full_keys = {
      "my_key1", "my_key2", "my_key3", "my_key4", "my_key5",
  };
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(full_keys);
  EXPECT_EQ(kv_pairs.size(), 2);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key4", "my_value"),
                                             KVPairEq("my_key5", "my_value")));
}

TEST(CleanUpTimestamps, CantInsertOldRecordsAfterCleanup) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  cache->UpdateKeyValue("my_key1", "my_value", 10);
  cache->DeleteKey("my_key1", 12);
  cache->RemoveDeletedKeys(13);

  auto deleted_nodes = KeyValueCacheTestPeer::ReadDeletedNodes(*cache);
  EXPECT_EQ(deleted_nodes.size(), 0);

  cache->UpdateKeyValue("my_key1", "my_value", 10);

  std::vector<std::string_view> keys = {"my_key1"};

  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(keys);
  EXPECT_EQ(kv_pairs.size(), 0);
}

TEST(CleanUpTimestampsForSetCache, InsertKeyValueSetDoesntUpdateDeletedNodes) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"my_value"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 1);
  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 0);
}

TEST(CleanUpTimestampsForSetCache, DeleteKeyValueSetExpectUpdateDeletedNodes) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"my_value"};
  cache->DeleteValuesInSet("my_key", absl::Span<std::string_view>(values), 1);
  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 1);
  EXPECT_EQ(KeyValueCacheTestPeer::ReadDeletedSetNodesForTimestamp(*cache, 1,
                                                                   "my_key")
                .size(),
            1);
}

TEST(CleanUpTimestampsForSetCache, RemoveDeletedKeyValuesRemovesOldRecords) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"my_value"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet("my_key", absl::Span<std::string_view>(values), 2);
  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 1);

  cache->RemoveDeletedKeys(3);
  deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 0);
  EXPECT_EQ(KeyValueCacheTestPeer::GetCacheKeyValueSetMapSize(*cache), 0);
}

TEST(CleanUpTimestampsForSetCache,
     RemoveDeletedKeyValuesDoesntAffectNewRecords) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"my_value"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 5);
  cache->DeleteValuesInSet("my_key", absl::Span<std::string_view>(values), 6);

  cache->RemoveDeletedKeys(2);

  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 1);
  EXPECT_EQ(KeyValueCacheTestPeer::ReadDeletedSetNodesForTimestamp(*cache, 6,
                                                                   "my_key")
                .size(),
            1);
}

TEST(CleanUpTimestampsForSetCache,
     RemoveDeletedKeysRemovesOldRecordsDoesntAffectNewRecords) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"v1", "v2"};
  std::vector<std::string_view> values_to_delete = {"v1"};
  cache->UpdateKeyValueSet("my_key1", absl::Span<std::string_view>(values), 1);
  cache->UpdateKeyValueSet("my_key2", absl::Span<std::string_view>(values), 2);
  cache->UpdateKeyValueSet("my_key3", absl::Span<std::string_view>(values), 3);
  cache->UpdateKeyValueSet("my_key4", absl::Span<std::string_view>(values), 4);

  cache->DeleteValuesInSet("my_key3",
                           absl::Span<std::string_view>(values_to_delete), 4);
  cache->DeleteValuesInSet("my_key1",
                           absl::Span<std::string_view>(values_to_delete), 5);
  cache->DeleteValuesInSet("my_key2",
                           absl::Span<std::string_view>(values_to_delete), 6);

  cache->RemoveDeletedKeys(5);

  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 1);
  EXPECT_EQ(KeyValueCacheTestPeer::ReadDeletedSetNodesForTimestamp(*cache, 6,
                                                                   "my_key2")
                .size(),
            1);
  auto get_value_set_result =
      cache->GetKeyValueSet({"my_key1", "my_key4", "my_key3"});
  EXPECT_THAT(get_value_set_result->GetValueSet("my_key4"),
              UnorderedElementsAre("v1", "v2"));
  EXPECT_THAT(get_value_set_result->GetValueSet("my_key3"),
              UnorderedElementsAre("v2"));
  EXPECT_THAT(get_value_set_result->GetValueSet("my_key1"),
              UnorderedElementsAre("v2"));
}

TEST(CleanUpTimestampsForSetCache, CantInsertOldRecordsAfterCleanup) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"my_value"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet("my_key", absl::Span<std::string_view>(values), 2);
  cache->RemoveDeletedKeys(3);

  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 0);
  EXPECT_EQ(KeyValueCacheTestPeer::GetCacheKeyValueSetMapSize(*cache), 0);

  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 2);

  absl::flat_hash_set<std::string_view> kv_set =
      cache->GetKeyValueSet({"my_key"})->GetValueSet("my_key");
  EXPECT_EQ(kv_set.size(), 0);
}

TEST(CleanUpTimestampsForSetCache, CantAddOldDeletedRecordsAfterCleanup) {
  std::unique_ptr<KeyValueCache> cache = std::make_unique<KeyValueCache>();
  std::vector<std::string_view> values = {"my_value"};
  cache->UpdateKeyValueSet("my_key", absl::Span<std::string_view>(values), 1);
  cache->DeleteValuesInSet("my_key", absl::Span<std::string_view>(values), 2);
  cache->RemoveDeletedKeys(3);

  int deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 0);
  EXPECT_EQ(KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache), 0);

  // Old delete
  cache->DeleteValuesInSet("my_key", absl::Span<std::string_view>(values), 2);
  deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 0);
  EXPECT_EQ(KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache), 0);

  // New delete
  cache->DeleteValuesInSet("my_key", absl::Span<std::string_view>(values), 4);
  deleted_nodes_map_size =
      KeyValueCacheTestPeer::GetDeletedSetNodesMapSize(*cache);
  EXPECT_EQ(deleted_nodes_map_size, 1);
  EXPECT_EQ(KeyValueCacheTestPeer::GetCacheKeyValueSetMapSize(*cache), 1);
  auto value_meta =
      KeyValueCacheTestPeer::GetSetValueMeta(*cache, "my_key", "my_value");
  EXPECT_EQ(value_meta.is_deleted, true);
  EXPECT_EQ(value_meta.last_logical_commit_time, 4);

  absl::flat_hash_set<std::string_view> kv_set =
      cache->GetKeyValueSet({"my_key"})->GetValueSet("my_key");
  EXPECT_EQ(kv_set.size(), 0);
}

TEST(ConcurrentSetMemoryAccessTest, ConcurrentGetAndGet) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys_lookup_request = {"key1", "key2"};
  std::vector<std::string_view> values_for_key1 = {"v1"};
  std::vector<std::string_view> values_for_key2 = {"v2"};
  cache->UpdateKeyValueSet("key1",
                           absl::Span<std::string_view>(values_for_key1), 1);
  cache->UpdateKeyValueSet("key2",
                           absl::Span<std::string_view>(values_for_key2), 1);
  absl::Notification start;
  auto lookup_fn = [&cache, &keys_lookup_request, &start]() {
    start.WaitForNotification();
    auto result = cache->GetKeyValueSet(keys_lookup_request);
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

TEST(ConcurrentSetMemoryAccessTest, ConcurrentGetAndUpdateExpectNoUpdate) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1"};
  std::vector<std::string_view> existing_values = {"v1"};
  cache->UpdateKeyValueSet("key1",
                           absl::Span<std::string_view>(existing_values), 3);
  absl::Notification start;
  auto lookup_fn = [&cache, &keys, &start]() {
    start.WaitForNotification();
    EXPECT_THAT(cache->GetKeyValueSet(keys)->GetValueSet("key1"),
                UnorderedElementsAre("v1"));
  };
  std::vector<std::string_view> new_values = {"v1"};
  auto update_fn = [&cache, &new_values, &start]() {
    start.WaitForNotification();
    cache->UpdateKeyValueSet("key1", absl::Span<std::string_view>(new_values),
                             1);
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

TEST(ConcurrentSetMemoryAccessTest, ConcurrentGetAndUpdateExpectUpdate) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1", "key2"};
  std::vector<std::string_view> existing_values = {"v1"};
  cache->UpdateKeyValueSet("key1",
                           absl::Span<std::string_view>(existing_values), 1);
  absl::Notification start;
  auto lookup_fn = [&cache, &keys, &start]() {
    start.WaitForNotification();
    EXPECT_THAT(cache->GetKeyValueSet(keys)->GetValueSet("key1"),
                UnorderedElementsAre("v1"));
  };
  std::vector<std::string_view> new_values_for_key2 = {"v2"};
  auto update_fn = [&cache, &new_values_for_key2, &start]() {
    // expect new value is inserted for key2
    start.WaitForNotification();
    cache->UpdateKeyValueSet(
        "key2", absl::Span<std::string_view>(new_values_for_key2), 2);
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

TEST(ConcurrentSetMemoryAccessTest, ConcurrentGetAndDeleteExpectNoDelete) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1"};
  std::vector<std::string_view> existing_values = {"v1"};
  cache->UpdateKeyValueSet("key1",
                           absl::Span<std::string_view>(existing_values), 3);
  absl::Notification start;
  auto lookup_fn = [&cache, &keys, &start]() {
    start.WaitForNotification();
    EXPECT_THAT(cache->GetKeyValueSet(keys)->GetValueSet("key1"),
                UnorderedElementsAre("v1"));
  };
  std::vector<std::string_view> delete_values = {"v1"};
  auto delete_fn = [&cache, &delete_values, &start]() {
    // expect no delete
    start.WaitForNotification();
    cache->DeleteValuesInSet("key1",
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

TEST(ConcurrentSetMemoryAccessTest, ConcurrentGetAndCleanUp) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1", "key2"};
  std::vector<std::string_view> existing_values = {"v1"};
  cache->UpdateKeyValueSet("key1",
                           absl::Span<std::string_view>(existing_values), 3);
  cache->UpdateKeyValueSet("key2",
                           absl::Span<std::string_view>(existing_values), 1);
  cache->DeleteValuesInSet("key2",
                           absl::Span<std::string_view>(existing_values), 2);
  absl::Notification start;
  auto lookup_fn = [&cache, &keys, &start]() {
    start.WaitForNotification();
    EXPECT_THAT(cache->GetKeyValueSet(keys)->GetValueSet("key1"),
                UnorderedElementsAre("v1"));
    EXPECT_EQ(cache->GetKeyValueSet(keys)->GetValueSet("key2").size(), 0);
  };
  auto cleanup_fn = [&cache, &start]() {
    // clean up old records
    start.WaitForNotification();
    KeyValueCacheTestPeer::CallCacheCleanup(*cache, 3);
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

TEST(ConcurrentSetMemoryAccessTest, ConcurrentUpdateAndUpdateExpectUpdateBoth) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1", "key2"};
  std::vector<std::string_view> values_for_key1 = {"v1"};
  absl::Notification start;
  auto update_key1 = [&cache, &keys, &values_for_key1, &start]() {
    start.WaitForNotification();
    // expect new value is inserted for key1
    cache->UpdateKeyValueSet("key1",
                             absl::Span<std::string_view>(values_for_key1), 1);
    EXPECT_THAT(cache->GetKeyValueSet(keys)->GetValueSet("key1"),
                UnorderedElementsAre("v1"));
  };
  std::vector<std::string_view> values_for_key2 = {"v2"};
  auto update_key2 = [&cache, &keys, &values_for_key2, &start]() {
    // expect new value is inserted for key2
    start.WaitForNotification();
    cache->UpdateKeyValueSet("key2",
                             absl::Span<std::string_view>(values_for_key2), 2);
    EXPECT_THAT(cache->GetKeyValueSet(keys)->GetValueSet("key2"),
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

TEST(ConcurrentSetMemoryAccessTest, ConcurrentUpdateAndDelete) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1", "key2"};
  std::vector<std::string_view> values_for_key1 = {"v1"};
  absl::Notification start;
  auto update_key1 = [&cache, &keys, &values_for_key1, &start]() {
    start.WaitForNotification();
    // expect new value is inserted for key1
    cache->UpdateKeyValueSet("key1",
                             absl::Span<std::string_view>(values_for_key1), 1);
    EXPECT_THAT(cache->GetKeyValueSet(keys)->GetValueSet("key1"),
                UnorderedElementsAre("v1"));
  };
  // Update existing value for key2
  std::vector<std::string_view> existing_values_for_key2 = {"v1", "v2"};
  cache->UpdateKeyValueSet(
      "key2", absl::Span<std::string_view>(existing_values_for_key2), 1);
  std::vector<std::string_view> values_to_delete_for_key2 = {"v1"};

  auto delete_key2 = [&cache, &keys, &values_to_delete_for_key2, &start]() {
    start.WaitForNotification();
    // expect value is deleted for key2
    cache->DeleteValuesInSet(
        "key2", absl::Span<std::string_view>(values_to_delete_for_key2), 2);
    EXPECT_THAT(cache->GetKeyValueSet(keys)->GetValueSet("key2"),
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

TEST(ConcurrentSetMemoryAccessTest, ConcurrentUpdateAndCleanUp) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1"};
  std::vector<std::string_view> values_for_key1 = {"v1"};
  absl::Notification start;
  auto update_fn = [&cache, &keys, &values_for_key1, &start]() {
    start.WaitForNotification();
    cache->UpdateKeyValueSet("key1",
                             absl::Span<std::string_view>(values_for_key1), 1);
    EXPECT_THAT(cache->GetKeyValueSet(keys)->GetValueSet("key1"),
                UnorderedElementsAre("v1"));
  };
  auto cleanup_fn = [&cache, &start]() {
    start.WaitForNotification();
    KeyValueCacheTestPeer::CallCacheCleanup(*cache, 2);
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

TEST(ConcurrentSetMemoryAccessTest, ConcurrentDeleteAndCleanUp) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1"};
  std::vector<std::string_view> values_for_key1 = {"v1"};
  cache->UpdateKeyValueSet("key1",
                           absl::Span<std::string_view>(values_for_key1), 1);
  absl::Notification start;
  auto delete_fn = [&cache, &keys, &values_for_key1, &start]() {
    start.WaitForNotification();
    // expect new value is deleted for key1
    cache->DeleteValuesInSet("key1",
                             absl::Span<std::string_view>(values_for_key1), 2);
    EXPECT_EQ(cache->GetKeyValueSet(keys)->GetValueSet("key1").size(), 0);
  };
  auto cleanup_fn = [&cache, &start]() {
    start.WaitForNotification();
    KeyValueCacheTestPeer::CallCacheCleanup(*cache, 2);
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

TEST(ConcurrentSetMemoryAccessTest, ConcurrentGetUpdateDeleteCleanUp) {
  auto cache = std::make_unique<KeyValueCache>();
  absl::flat_hash_set<std::string_view> keys = {"key1", "key2"};
  std::vector<std::string_view> existing_values_for_key1 = {"v1"};
  std::vector<std::string_view> existing_values_for_key2 = {"v1"};
  cache->UpdateKeyValueSet(
      "key1", absl::Span<std::string_view>(existing_values_for_key1), 1);
  cache->UpdateKeyValueSet(
      "key2", absl::Span<std::string_view>(existing_values_for_key2), 1);

  std::vector<std::string_view> values_to_insert_for_key2 = {"v2"};
  std::vector<std::string_view> values_to_delete_for_key2 = {"v1"};
  absl::Notification start;
  auto insert_for_key2 = [&cache, &values_to_insert_for_key2, &start]() {
    start.WaitForNotification();
    cache->UpdateKeyValueSet(
        "key2", absl::Span<std::string_view>(values_to_insert_for_key2), 2);
  };
  auto delete_for_key2 = [&cache, &values_to_delete_for_key2, &start]() {
    start.WaitForNotification();
    cache->DeleteValuesInSet(
        "key2", absl::Span<std::string_view>(values_to_delete_for_key2), 2);
  };
  auto cleanup = [&cache, &start]() {
    start.WaitForNotification();
    KeyValueCacheTestPeer::CallCacheCleanup(*cache, 2);
  };

  auto lookup_for_key1 = [&cache, &keys, &start]() {
    start.WaitForNotification();
    EXPECT_THAT(cache->GetKeyValueSet(keys)->GetValueSet("key1"),
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
      cache->GetKeyValueSet(keys)->GetValueSet("key2");
  EXPECT_THAT(look_up_result_for_key2, UnorderedElementsAre("v2"));
}
}  // namespace
}  // namespace kv_server
