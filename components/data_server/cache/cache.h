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

#ifndef COMPONENTS_DATA_SERVER_CACHE_CACHE_H_
#define COMPONENTS_DATA_SERVER_CACHE_CACHE_H_

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "public/base_types.pb.h"

namespace kv_server {

// Interface for in-memory datastore.
// One cache object is only for keys in one namespace.
// ShardedCache should be used for multiple key namespaces. See below.
class Cache {
 public:
  struct FullyQualifiedKey {
    std::string_view key;
    std::string_view subkey;
  };

  virtual ~Cache() = default;

  // Looks up and returns key-value pairs for the given keys.
  virtual std::vector<std::pair<FullyQualifiedKey, std::string>>
  GetKeyValuePairs(
      const std::vector<FullyQualifiedKey>& full_key_list) const = 0;

  // Inserts or updates the key, subkey with the new value.
  virtual void UpdateKeyValue(FullyQualifiedKey full_key,
                              std::string value) = 0;

  // Deletes a particular (key, subkey) tuple.
  virtual void DeleteKey(FullyQualifiedKey full_key) = 0;

  static std::unique_ptr<Cache> Create();
};

// Manages all caches for all key namespaces.
// For the best concurrent performance, all caches are initialized at creation
// time and no API is provided for adding/removing caches.
class ShardedCache {
 public:
  static std::unique_ptr<ShardedCache> Create();

  virtual ~ShardedCache() = default;
  virtual Cache& GetMutableCacheShard(KeyNamespace::Enum key_namespace) = 0;
  virtual const Cache& GetCacheShard(
      KeyNamespace::Enum key_namespace) const = 0;
};

inline std::ostream& operator<<(std::ostream& os,
                                const Cache::FullyQualifiedKey& full_key) {
  os << full_key.key << ":" << full_key.subkey;
  return os;
}

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_CACHE_CACHE_H_
