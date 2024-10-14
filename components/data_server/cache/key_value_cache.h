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

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/get_key_value_set_result.h"
#include "components/data_server/cache/uint_value_set.h"
#include "components/data_server/cache/uint_value_set_cache.h"

namespace kv_server {
// In-memory datastore.
// One cache object is only for keys in one namespace.
class KeyValueCache : public Cache {
 public:
  // Looks up and returns key-value pairs for the given keys.
  absl::flat_hash_map<std::string, std::string> GetKeyValuePairs(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const override;

  // Looks up and returns key-value set result for the given key set.
  std::unique_ptr<GetKeyValueSetResult> GetKeyValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const override;

  // Looks up and returns int32 value set result for the given key set.
  std::unique_ptr<GetKeyValueSetResult> GetUInt32ValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const override;

  std::unique_ptr<GetKeyValueSetResult> GetUInt64ValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const override;

  // Inserts or updates the key with the new value for a given prefix
  void UpdateKeyValue(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, std::string_view value, int64_t logical_commit_time,
      std::string_view prefix = "") override;

  // Inserts or updates values in the set for a given key and prefix, if a value
  // exists, updates its timestamp to the latest logical commit time.
  void UpdateKeyValueSet(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, absl::Span<std::string_view> input_value_set,
      int64_t logical_commit_time, std::string_view prefix = "") override;

  // Inserts or updates values in the set for a given key and prefix, if a value
  // exists, updates its timestamp to the latest logical commit time.
  void UpdateKeyValueSet(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, absl::Span<uint32_t> value_set,
      int64_t logical_commit_time, std::string_view prefix = "") override;

  void UpdateKeyValueSet(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, absl::Span<uint64_t> value_set,
      int64_t logical_commit_time, std::string_view prefix = "") override;

  // Deletes a particular (key, value) pair for a given prefix.
  void DeleteKey(privacy_sandbox::server_common::log::PSLogContext& log_context,
                 std::string_view key, int64_t logical_commit_time,
                 std::string_view prefix = "") override;

  // Deletes values in the set for a given key and prefix. The deletion, this
  // object still exist and is marked "deleted", in case there are late-arriving
  // updates to this value.
  void DeleteValuesInSet(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, absl::Span<std::string_view> value_set,
      int64_t logical_commit_time, std::string_view prefix = "") override;

  // Deletes values in the set for a given key and prefix. The deletion, this
  // object still exist and is marked "deleted", in case there are late-arriving
  // updates to this value.
  void DeleteValuesInSet(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, absl::Span<uint32_t> value_set,
      int64_t logical_commit_time, std::string_view prefix = "") override;

  void DeleteValuesInSet(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, absl::Span<uint64_t> value_set,
      int64_t logical_commit_time, std::string_view prefix = "") override;

  // Removes the values that were deleted before the specified
  // logical_commit_time for a given prefix.
  // TODO: b/267182790 -- Cache cleanup should be done periodically from a
  // background thread
  void RemoveDeletedKeys(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      int64_t logical_commit_time, std::string_view prefix = "") override;

  static std::unique_ptr<Cache> Create();

 private:
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
  struct SetValueMeta {
    // Last logical commit time for a value
    int64_t last_logical_commit_time;
    // Boolean to mark if the value should be deleted or not.
    // We need this to represent its deleted state,
    // because after deletion, this value should still exist in case
    // there are late-arriving updates to this.
    bool is_deleted;
    SetValueMeta() : last_logical_commit_time(0), is_deleted(false) {}
    SetValueMeta(int64_t logical_commit_time, bool deleted)
        : last_logical_commit_time(logical_commit_time), is_deleted(deleted) {}
  };

  // Removes deleted keys from key-value map for a given prefix
  void CleanUpKeyValueMap(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      int64_t logical_commit_time, std::string_view prefix);

  // Removes deleted key-values from key-value_set map for a given prefix
  void CleanUpKeyValueSetMap(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      int64_t logical_commit_time, std::string_view prefix);

  void CleanUpUIntSetMaps(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      int64_t logical_commit_time, std::string_view prefix);

  // Logs cache access metrics for cache hit or miss counts. The cache access
  // event name is defined in server_definition.h file
  void LogCacheAccessMetrics(const RequestContext& request_context,
                             std::string_view cache_access_event) const;

  // mutex for key value map;
  mutable absl::Mutex mutex_;
  // mutex for key value set map;
  mutable absl::Mutex set_map_mutex_;
  // Mapping from a key to its value
  absl::flat_hash_map<std::string, CacheValue> map_ ABSL_GUARDED_BY(mutex_);

  // Sorted mapping from the logical timestamp to a key, for nodes that were
  // deleted We keep this to do proper and efficient clean up in map_.
  // The key in the inner map is the prefix and the value is the keys of key
  // value pairs deleted from that prefix

  absl::flat_hash_map<std::string, std::multimap<int64_t, std::string>>
      deleted_nodes_map_ ABSL_GUARDED_BY(mutex_);

  // The key is the prefix and the value is the
  // maximum timestamp that was passed to RemoveDeletedKeys.
  absl::flat_hash_map<std::string, int64_t> max_cleanup_logical_commit_time_map_
      ABSL_GUARDED_BY(mutex_);

  // The key is the prefix and the value is the maximum
  // logical commit time that is used to do update/delete for key-value set map.
  // TODO(b/284474892) Need to evaluate if we really need to make this variable
  //  guarded b mutex, if not, we may want to remove it and use one
  // max_cleanup_logical_commit_time in update/deletion for both maps
  absl::flat_hash_map<std::string, int64_t>
      set_cache_max_cleanup_logical_commit_time_
          ABSL_GUARDED_BY(set_map_mutex_);

  // Mapping from a key to its value map. The key in the inner map is the
  // value string, and value is the ValueMeta. The inner map allows value
  // look up to check the meta data to determine to state of the value
  // in the cache, like logical commit time and whether the value
  // is deleted or not.
  absl::flat_hash_map<
      std::string,
      std::unique_ptr<std::pair<
          absl::Mutex, absl::flat_hash_map<std::string, SetValueMeta>>>>
      key_to_value_set_map_ ABSL_GUARDED_BY(set_map_mutex_);
  // The key of outer map is the prefix, and value is the sorted mapping
  // from logical timestamp to key-value_set map to keep track of
  // deleted key-values to handle out of order update case. In the inner map,
  // the key string is the key for the values, and the string
  // in the flat_hash_set is the value
  absl::flat_hash_map<
      std::string,
      absl::btree_map<
          int64_t,
          absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>>>
      deleted_set_nodes_map_ ABSL_GUARDED_BY(set_map_mutex_);

  UIntValueSetCache<UInt32ValueSet> uint32_sets_cache_;
  UIntValueSetCache<UInt64ValueSet> uint64_sets_cache_;

  friend class KeyValueCacheTestPeer;
};
}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_CACHE_KEY_VALUE_CACHE_H_
