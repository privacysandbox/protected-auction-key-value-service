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
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "components/data_server/cache/cache.h"
#include "public/base_types.pb.h"

namespace fledge::kv_server {
namespace {

struct KeyContent {
  // The default value should normally be set, but it is not enforced in any
  // way. If the operator updates a subkey before updating the default value,
  // this could be unset.
  std::optional<std::string> default_value;
  absl::flat_hash_map<std::string, std::string> subkey_values;
};

// Mapping from a key to its values with subkey overrides.
using KeyContentMap = absl::flat_hash_map<std::string, KeyContent>;

class KeyValueCache : public Cache {
 public:
  std::vector<std::pair<FullyQualifiedKey, std::string>> GetKeyValuePairs(
      const std::vector<Cache::FullyQualifiedKey>& full_key_list)
      const override {
    std::vector<std::pair<Cache::FullyQualifiedKey, std::string>> kv_pairs;

    absl::ReaderMutexLock lock(&mutex_);
    for (const auto& full_key : full_key_list) {
      auto key_iter = map_.find(full_key.key);
      if (key_iter == map_.end()) {
        continue;
      }
      if (auto subkey_iter =
              key_iter->second.subkey_values.find(full_key.subkey);
          subkey_iter != key_iter->second.subkey_values.end()) {
        kv_pairs.emplace_back(full_key, subkey_iter->second);
      } else if (ABSL_PREDICT_TRUE(
                     key_iter->second.default_value.has_value())) {
        // Fall back to use the default value
        kv_pairs.emplace_back(full_key, key_iter->second.default_value.value());
      }
    }
    return kv_pairs;
  }

  // Replaces the current key-value entry with the new key-value entry.
  void UpdateKeyValue(FullyQualifiedKey full_key, std::string value) override {
    const bool no_subkey = full_key.subkey.empty();

    absl::MutexLock lock(&mutex_);
    KeyContent& key_content = map_[full_key.key];
    if (no_subkey) {
      key_content.default_value = std::move(value);
    } else {
      key_content.subkey_values.insert_or_assign(full_key.subkey,
                                                 std::move(value));
    }
  }

  void DeleteKey(FullyQualifiedKey full_key) override {
    absl::MutexLock lock(&mutex_);
    if (auto key_iter = map_.find(full_key.key); key_iter != map_.end()) {
      if (full_key.subkey.empty()) {
        map_.erase(key_iter);
      } else {
        key_iter->second.subkey_values.erase(full_key.subkey);
      }
    }
  }

 private:
  mutable absl::Mutex mutex_;

  KeyContentMap map_ ABSL_GUARDED_BY(mutex_);
};

// Since no edition can be done to the array, no lock is used.
// This expects the underlying cache object itself is thread safe.
class NamespaceShardedCache : public ShardedCache {
 public:
  NamespaceShardedCache()
      : caches_([] {
          CacheArrayType raw_cache;
          for (auto& shard : raw_cache) {
            shard = Cache::Create();
          }
          return raw_cache;
        }()) {}

  Cache& GetMutableCacheShard(KeyNamespace::Enum key_namespace) override {
    return *caches_[key_namespace];
  }
  const Cache& GetCacheShard(KeyNamespace::Enum key_namespace) const override {
    return *caches_[key_namespace];
  }

 private:
  using CacheArrayType =
      std::array<std::unique_ptr<Cache>, KeyNamespace::Enum_ARRAYSIZE>;
  CacheArrayType caches_;
};

}  // namespace

std::unique_ptr<Cache> Cache::Create() {
  return std::make_unique<KeyValueCache>();
}

std::unique_ptr<ShardedCache> ShardedCache::Create() {
  return std::make_unique<NamespaceShardedCache>();
}

}  // namespace fledge::kv_server
