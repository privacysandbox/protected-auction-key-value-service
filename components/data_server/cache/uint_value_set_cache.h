/*
 * Copyright 2024 Google LLC
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

#ifndef COMPONENTS_DATA_SERVER_CACHE_UINT_VALUE_SET_CACHE_H_
#define COMPONENTS_DATA_SERVER_CACHE_UINT_VALUE_SET_CACHE_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/container/btree_map.h"
#include "components/container/thread_safe_hash_map.h"
#include "components/data_server/cache/get_key_value_set_result.h"
#include "components/util/request_context.h"
#include "src/logger/request_context_logger.h"

namespace kv_server {

template <typename SetType>
class UIntValueSetCache {
 public:
  // Returns "uint" value set result for given set keys.
  std::unique_ptr<GetKeyValueSetResult> GetValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const;

  // Inserts or updates set values for a given key and prefix. If a value
  // exists, updates its timestamp to the latest logical commit time.
  void UpdateSetValues(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, absl::Span<typename SetType::value_type> value_set,
      int64_t logical_commit_time, std::string_view prefix = "");

  // Deletes set values for a given key and prefix. After the deletion,
  // the values still exist and is marked "deleted", in case there are
  // late-arriving updates to this value.
  void DeleteSetValues(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, absl::Span<typename SetType::value_type> value_set,
      int64_t logical_commit_time, std::string_view prefix = "");

  // Removes the set values that were deleted before the specified
  // logical_commit_time for a given prefix, i.e., actually reclaims the space
  // used by deleted values.
  void CleanUpValueSets(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      int64_t logical_commit_time, std::string_view prefix = "");

 private:
  // Maps set key to unsigned int value set per prefix.
  ThreadSafeHashMap<std::string, SetType> sets_map_;
  // Maps prefix to maximum clean up commit time. Set updates for this prefix
  // before maximum clean up commit time are ignored.
  ThreadSafeHashMap<std::string, int64_t> sets_max_cleanup_commit_time_map_;
  // Maps prefix to list of set keys with deleted elements grouped by deletion
  // timestamp.
  ThreadSafeHashMap<std::string,
                    absl::btree_map<int64_t, absl::flat_hash_set<std::string>>>
      deleted_sets_map_;

  // Allow a unit test class to access private members to verify correct
  // deletion and clean up.
  friend class UIntValueSetCacheTest;
};

template <typename SetType>
std::unique_ptr<GetKeyValueSetResult> UIntValueSetCache<SetType>::GetValueSet(
    const RequestContext& request_context,
    const absl::flat_hash_set<std::string_view>& key_set) const {
  auto result = GetKeyValueSetResult::Create();
  for (auto key : key_set) {
    PS_VLOG(8, request_context.GetPSLogContext()) << "Getting key: " << key;
    result->AddUIntValueSet(key, sets_map_.CGet(key));
  }
  return result;
}

template <typename SetType>
void UIntValueSetCache<SetType>::UpdateSetValues(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    std::string_view key, absl::Span<typename SetType::value_type> value_set,
    int64_t logical_commit_time, std::string_view prefix) {
  if (value_set.empty()) {
    PS_VLOG(8, log_context)
        << "Skipping the update as it has no value in the input set.";
    return;
  }
  if (auto prefix_max_time_node =
          sets_max_cleanup_commit_time_map_.CGet(prefix);
      prefix_max_time_node.is_present() &&
      logical_commit_time <= *prefix_max_time_node.value()) {
    PS_VLOG(8, log_context)
        << "Skipping the update as its logical_commit_time: "
        << logical_commit_time << " is older than the current cutoff time:"
        << *prefix_max_time_node.value();
    return;  // Skip old updates.
  }
  auto cached_set_node = sets_map_.Get(key);
  if (!cached_set_node.is_present()) {
    auto result = sets_map_.PutIfAbsent(key, SetType());
    if (result.second) {
      PS_VLOG(8, log_context) << "Added new key: [" << key << "] is a new key.";
    }
    cached_set_node = std::move(result.first);
  }
  cached_set_node.value()->Add(value_set, logical_commit_time);
}

template <typename SetType>
void UIntValueSetCache<SetType>::DeleteSetValues(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    std::string_view key, absl::Span<typename SetType::value_type> value_set,
    int64_t logical_commit_time, std::string_view prefix) {
  if (value_set.empty()) {
    PS_VLOG(8, log_context)
        << "Skipping the delete as it has no value in the input set.";
    return;
  }
  if (auto prefix_max_time_node =
          sets_max_cleanup_commit_time_map_.CGet(prefix);
      prefix_max_time_node.is_present() &&
      logical_commit_time <= *prefix_max_time_node.value()) {
    PS_VLOG(1, log_context)
        << "Skipping the delete as its logical_commit_time: "
        << logical_commit_time << " is older than the current cutoff time:"
        << *prefix_max_time_node.value();
    return;  // Skip old deletes.
  }
  {
    auto cached_set_node = sets_map_.Get(key);
    if (!cached_set_node.is_present()) {
      auto result = sets_map_.PutIfAbsent(key, SetType());
      cached_set_node = std::move(result.first);
    }
    cached_set_node.value()->Remove(value_set, logical_commit_time);
  }
  {
    // Mark set as having deleted elements.
    auto prefix_deleted_sets_node = deleted_sets_map_.Get(prefix);
    if (!prefix_deleted_sets_node.is_present()) {
      auto result = deleted_sets_map_.PutIfAbsent(
          prefix, absl::btree_map<int64_t, absl::flat_hash_set<std::string>>());
      prefix_deleted_sets_node = std::move(result.first);
    }
    auto* commit_time_sets =
        &(*prefix_deleted_sets_node.value())[logical_commit_time];
    commit_time_sets->insert(std::string(key));
  }
}

template <typename SetType>
void UIntValueSetCache<SetType>::CleanUpValueSets(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    int64_t logical_commit_time, std::string_view prefix) {
  {
    if (auto max_cleanup_time_node =
            sets_max_cleanup_commit_time_map_.PutIfAbsent(prefix,
                                                          logical_commit_time);
        *max_cleanup_time_node.first.value() < logical_commit_time) {
      *max_cleanup_time_node.first.value() = logical_commit_time;
    } else if (logical_commit_time < *max_cleanup_time_node.first.value()) {
      return;
    }
  }
  absl::flat_hash_set<std::string> cleanup_sets;
  {
    auto prefix_deleted_sets_node = deleted_sets_map_.Get(prefix);
    if (!prefix_deleted_sets_node.is_present()) {
      return;  // nothing to cleanup for this prefix.
    }
    absl::flat_hash_set<int64_t> cleanup_commit_times;
    for (const auto& [commit_time, deleted_sets] :
         *prefix_deleted_sets_node.value()) {
      if (commit_time > logical_commit_time) {
        break;
      }
      cleanup_commit_times.insert(commit_time);
      cleanup_sets.insert(deleted_sets.begin(), deleted_sets.end());
    }
    for (auto commit_time : cleanup_commit_times) {
      prefix_deleted_sets_node.value()->erase(
          prefix_deleted_sets_node.value()->find(commit_time));
    }
  }
  {
    for (const auto& set : cleanup_sets) {
      if (auto set_node = sets_map_.Get(set); set_node.is_present()) {
        set_node.value()->Cleanup(logical_commit_time);
      }
    }
  }
}

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_CACHE_UINT_VALUE_SET_CACHE_H_
