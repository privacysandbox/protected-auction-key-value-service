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

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "components/data_server/cache/cache.h"
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
  static absl::flat_hash_map<std::string, CacheValue>& ReadNodes(
      KeyValueCache& c) {
    absl::MutexLock lock(&c.mutex_);
    return c.map_;
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

TEST(DeleteKeyTest, RemovesKeyEntry) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue("my_key", "my_value", 1);
  cache->DeleteKey("my_key", 2);
  std::vector<std::string_view> full_keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(full_keys);
  EXPECT_EQ(kv_pairs.size(), 0);
}

TEST(DeleteKeyTest, WrongkeyDoesNotRemoveEntry) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue("my_key", "my_value", 1);
  cache->DeleteKey("wrong_key", 1);
  std::vector<std::string_view> keys = {"my_key"};
  absl::flat_hash_map<std::string, std::string> kv_pairs =
      cache->GetKeyValuePairs(keys);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
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
  EXPECT_EQ(kv_pairs.size(), 1);
  EXPECT_THAT(kv_pairs, UnorderedElementsAre(KVPairEq("my_key", "my_value")));
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

}  // namespace
}  // namespace kv_server
