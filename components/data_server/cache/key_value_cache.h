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

#ifndef COMPONENTS_DATA_SERVER_CACHE_KEY_VALUE_CACHE_H_
#define COMPONENTS_DATA_SERVER_CACHE_KEY_VALUE_CACHE_H_

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "components/data_server/cache/cache.h"
#include "public/base_types.pb.h"

namespace kv_server {
struct CacheValue {
  // We need to be able to set the value to null. For deletion we're keeping
  // the timestamp of the key (to prevent a specific type of out of order
  // delete-update messages issue) until it is later cleaned up.
  // We've also considered using optional, but it takes more space.
  // sizeof(string) + sizeof(bool) -- for optional
  // sizeof(string*) when null, sizeof(string*) + sizeof(string) otherwise
  // -- for the unique pointer
  std::unique_ptr<std::string> value;
  int64_t last_logical_commit_time;
};

// In-memory datastore.
// One cache object is only for keys in one namespace.
class KeyValueCache : public Cache {
 public:
  // Looks up and returns key-value pairs for the given keys.
  std::vector<std::pair<std::string_view, std::string>> GetKeyValuePairs(
      const std::vector<std::string_view>& key_list) const override;

  // Inserts or updates the key with the new value.
  void UpdateKeyValue(std::string_view key, std::string_view value,
                      int64_t logical_commit_time) override;

  // Deletes a particular (key, value) pair.
  void DeleteKey(std::string_view key, int64_t logical_commit_time) override;

  // Remove the values that were deleted before the specified
  // logical_commit_time.
  // TODO: b/267182790 -- Cache cleanup should be done periodically from a
  // background thread
  void RemoveDeletedKeys(int64_t logical_commit_time) override;

  static std::unique_ptr<Cache> Create();

 private:
  mutable absl::Mutex mutex_;
  // Mapping from a key to its value
  absl::flat_hash_map<std::string, CacheValue> map_ ABSL_GUARDED_BY(mutex_);

  // Sorted mapping from the logical timestamp to a key, for nodes that were
  // deleted We keep this to do proper and efficient clean up in map_.
  absl::btree_map<int64_t, std::string> deleted_nodes_ ABSL_GUARDED_BY(mutex_);

  // The maximum value that was passed to RemoveDeletedKeys.
  int64_t max_cleanup_logical_commit_time_ ABSL_GUARDED_BY(mutex_) = 0;

  friend class KeyValueCacheTestPeer;
};
}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_CACHE_KEY_VALUE_CACHE_H_
