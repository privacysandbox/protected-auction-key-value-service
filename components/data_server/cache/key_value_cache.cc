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
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/get_key_value_set_result.h"

namespace kv_server {

absl::flat_hash_map<std::string, std::string> KeyValueCache::GetKeyValuePairs(
    const RequestContext& request_context,
    const absl::flat_hash_set<std::string_view>& key_set) const {
  ScopeLatencyMetricsRecorder<InternalLookupMetricsContext,
                              kGetValuePairsLatencyInMicros>
      latency_recorder(request_context.GetInternalLookupMetricsContext());
  absl::flat_hash_map<std::string, std::string> kv_pairs;
  absl::ReaderMutexLock lock(&mutex_);
  for (std::string_view key : key_set) {
    const auto key_iter = map_.find(key);
    if (key_iter == map_.end() || key_iter->second.value == nullptr) {
      continue;
    } else {
      PS_VLOG(9, request_context.GetPSLogContext())
          << "Get called for " << key
          << ". returning value: " << *(key_iter->second.value);
      kv_pairs.insert_or_assign(key, *(key_iter->second.value));
    }
  }
  if (kv_pairs.empty()) {
    LogCacheAccessMetrics(request_context, kKeyValueCacheMiss);
  } else {
    LogCacheAccessMetrics(request_context, kKeyValueCacheHit);
  }
  return kv_pairs;
}

std::unique_ptr<GetKeyValueSetResult> KeyValueCache::GetKeyValueSet(
    const RequestContext& request_context,
    const absl::flat_hash_set<std::string_view>& key_set) const {
  ScopeLatencyMetricsRecorder<InternalLookupMetricsContext,
                              kGetKeyValueSetLatencyInMicros>
      latency_recorder(request_context.GetInternalLookupMetricsContext());
  // lock the cache map
  absl::ReaderMutexLock lock(&set_map_mutex_);
  auto result = GetKeyValueSetResult::Create();
  bool cache_hit = false;
  for (const auto& key : key_set) {
    PS_VLOG(8, request_context.GetPSLogContext()) << "Getting key: " << key;
    const auto key_itr = key_to_value_set_map_.find(key);
    if (key_itr != key_to_value_set_map_.end()) {
      absl::flat_hash_set<std::string_view> value_set;
      auto set_lock =
          std::make_unique<absl::ReaderMutexLock>(&key_itr->second->first);
      for (const auto& v : key_itr->second->second) {
        if (!v.second.is_deleted) {
          value_set.emplace(v.first);
        }
      }
      // Add key value set to the result
      result->AddKeyValueSet(key, std::move(value_set), std::move(set_lock));
      cache_hit = true;
    }
  }
  if (cache_hit) {
    LogCacheAccessMetrics(request_context, kKeyValueSetCacheHit);
  } else {
    LogCacheAccessMetrics(request_context, kKeyValueSetCacheMiss);
  }
  return result;
}

// Looks up and returns int32 value set result for the given key set.
std::unique_ptr<GetKeyValueSetResult> KeyValueCache::GetUInt32ValueSet(
    const RequestContext& request_context,
    const absl::flat_hash_set<std::string_view>& key_set) const {
  ScopeLatencyMetricsRecorder<InternalLookupMetricsContext,
                              kGetUInt32ValueSetLatencyInMicros>
      latency_recorder(request_context.GetInternalLookupMetricsContext());
  auto result = GetKeyValueSetResult::Create();
  for (const auto& key : key_set) {
    result->AddUInt32ValueSet(key, uint32_sets_map_.CGet(key));
  }
  return result;
}

// Replaces the current key-value entry with the new key-value entry.
void KeyValueCache::UpdateKeyValue(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    std::string_view key, std::string_view value, int64_t logical_commit_time,
    std::string_view prefix) {
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext, kUpdateKeyValueLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  PS_VLOG(9, log_context) << "Received update for [" << key << "] at "
                          << logical_commit_time
                          << ". value will be set to: " << value;
  absl::MutexLock lock(&mutex_);

  auto max_cleanup_logical_commit_time =
      max_cleanup_logical_commit_time_map_[prefix];

  if (logical_commit_time <= max_cleanup_logical_commit_time) {
    PS_VLOG(1, log_context)
        << "Skipping the update as its logical_commit_time: "
        << logical_commit_time << " is not newer than the current cutoff time:"
        << max_cleanup_logical_commit_time;

    return;
  }

  const auto key_iter = map_.find(key);

  if (key_iter != map_.end() &&
      key_iter->second.last_logical_commit_time >= logical_commit_time) {
    PS_VLOG(1, log_context)
        << "Skipping the update as its logical_commit_time: "
        << logical_commit_time << " is not newer than the current value's time:"
        << key_iter->second.last_logical_commit_time;
    return;
  }

  if (key_iter != map_.end() &&
      key_iter->second.last_logical_commit_time < logical_commit_time &&
      key_iter->second.value == nullptr) {
    // should always have this, but checking just in case

    if (auto prefix_deleted_nodes_iter = deleted_nodes_map_.find(prefix);
        prefix_deleted_nodes_iter != deleted_nodes_map_.end()) {
      auto dl_key_iter = prefix_deleted_nodes_iter->second.find(
          key_iter->second.last_logical_commit_time);
      if (dl_key_iter != prefix_deleted_nodes_iter->second.end() &&
          dl_key_iter->second == key) {
        prefix_deleted_nodes_iter->second.erase(dl_key_iter);
      }
    }
  }

  map_.insert_or_assign(key, {.value = std::make_unique<std::string>(value),
                              .last_logical_commit_time = logical_commit_time});
}

void KeyValueCache::UpdateKeyValueSet(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    std::string_view key, absl::Span<std::string_view> input_value_set,
    int64_t logical_commit_time, std::string_view prefix) {
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext,
                              kUpdateKeyValueSetLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  PS_VLOG(9, log_context) << "Received update for [" << key << "] at "
                          << logical_commit_time;
  std::unique_ptr<absl::MutexLock> key_lock;
  absl::flat_hash_map<std::string, SetValueMeta>* existing_value_set;
  // The max cleanup time needs to be locked before doing this comparison
  {
    absl::MutexLock lock_map(&set_map_mutex_);

    auto max_cleanup_logical_commit_time =
        set_cache_max_cleanup_logical_commit_time_[prefix];

    if (logical_commit_time <= max_cleanup_logical_commit_time) {
      PS_VLOG(1, log_context)
          << "Skipping the update as its logical_commit_time: "
          << logical_commit_time << " is older than the current cutoff time:"
          << max_cleanup_logical_commit_time;
      return;
    } else if (input_value_set.empty()) {
      PS_VLOG(1, log_context)
          << "Skipping the update as it has no value in the set.";
      return;
    }
    auto key_itr = key_to_value_set_map_.find(key);
    if (key_itr == key_to_value_set_map_.end()) {
      PS_VLOG(9, log_context) << key << " is a new key. Adding it";
      // There is no existing value set for the given key,
      // simply insert the key value set to the map, no need to update deleted
      // set nodes
      auto mutex_value_map_pair = std::make_unique<std::pair<
          absl::Mutex, absl::flat_hash_map<std::string, SetValueMeta>>>();

      for (const auto& value : input_value_set) {
        mutex_value_map_pair->second.emplace(
            value, SetValueMeta{logical_commit_time, /*is_deleted=*/false});
      }
      key_to_value_set_map_.emplace(key, std::move(mutex_value_map_pair));
      return;
    }
    // The given key has an existing value set, then
    // update the existing value if update is suggested by the comparison result
    // on the logical commit times.
    // Lock the key
    key_lock = std::make_unique<absl::MutexLock>(&key_itr->second->first);
    existing_value_set = &key_itr->second->second;
  }  // end locking map;

  for (const auto& value : input_value_set) {
    auto& current_value_state = (*existing_value_set)[value];
    if (current_value_state.last_logical_commit_time >= logical_commit_time) {
      // no need to update
      continue;
    }
    // Insert new value or update existing value with
    // the recent logical commit time. If the existing value was marked
    // deleted, update is_deleted boolean to false
    current_value_state.is_deleted = false;
    current_value_state.last_logical_commit_time = logical_commit_time;
  }
  // end locking key
}
void KeyValueCache::UpdateKeyValueSet(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    std::string_view key, absl::Span<uint32_t> value_set,
    int64_t logical_commit_time, std::string_view prefix) {
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext,
                              kUpdateUInt32ValueSetLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  if (auto prefix_max_time_node =
          uint32_sets_max_cleanup_commit_time_map_.CGet(prefix);
      prefix_max_time_node.is_present() &&
      logical_commit_time <= *prefix_max_time_node.value()) {
    return;  // Skip old updates.
  }
  auto cached_set_node = uint32_sets_map_.Get(key);
  if (!cached_set_node.is_present()) {
    auto result = uint32_sets_map_.PutIfAbsent(key, UInt32ValueSet());
    cached_set_node = std::move(result.first);
  }
  cached_set_node.value()->Add(value_set, logical_commit_time);
}

void KeyValueCache::DeleteKey(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    std::string_view key, int64_t logical_commit_time,
    std::string_view prefix) {
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext, kDeleteKeyLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  absl::MutexLock lock(&mutex_);
  auto max_cleanup_logical_commit_time =
      max_cleanup_logical_commit_time_map_[prefix];
  if (logical_commit_time <= max_cleanup_logical_commit_time) {
    return;
  }
  const auto key_iter = map_.find(key);
  if ((key_iter != map_.end() &&
       key_iter->second.last_logical_commit_time < logical_commit_time) ||
      key_iter == map_.end()) {
    // If key is missing, we still need to add a null value to the map to
    // avoid the late coming update with smaller logical commit time
    // inserting value to the map for the given key
    map_.insert_or_assign(
        key,
        {.value = nullptr, .last_logical_commit_time = logical_commit_time});
    deleted_nodes_map_[prefix].emplace(logical_commit_time, key);
  }
}

void KeyValueCache::DeleteValuesInSet(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    std::string_view key, absl::Span<std::string_view> value_set,
    int64_t logical_commit_time, std::string_view prefix) {
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext,
                              kDeleteValuesInSetLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  std::unique_ptr<absl::MutexLock> key_lock;
  absl::flat_hash_map<std::string, SetValueMeta>* existing_value_set;
  // The max cleanup time needs to be locked before doing this comparison
  {
    absl::MutexLock lock_map(&set_map_mutex_);
    auto max_cleanup_logical_commit_time =
        set_cache_max_cleanup_logical_commit_time_[prefix];
    if (logical_commit_time <= max_cleanup_logical_commit_time ||
        value_set.empty()) {
      return;
    }
    auto key_itr = key_to_value_set_map_.find(key);
    if (key_itr == key_to_value_set_map_.end()) {
      // If the key is missing, still need to add all the deleted values to the
      // map to avoid late arriving update with smaller logical commit time
      // inserting values same as the deleted ones for the key
      auto mutex_value_map_pair = std::make_unique<std::pair<
          absl::Mutex, absl::flat_hash_map<std::string, SetValueMeta>>>();

      for (const auto& value : value_set) {
        mutex_value_map_pair->second.emplace(
            value, SetValueMeta{logical_commit_time, /*is_deleted=*/true});
      }
      key_to_value_set_map_.emplace(key, std::move(mutex_value_map_pair));
      // Add to deleted set nodes
      for (const std::string_view value : value_set) {
        deleted_set_nodes_map_[prefix][logical_commit_time][key].emplace(value);
      }
      return;
    }
    // Lock the key
    key_lock = std::make_unique<absl::MutexLock>(&key_itr->second->first);
    existing_value_set = &key_itr->second->second;
  }  // end locking map
  // Keep track of the values to be added to the deleted set nodes
  std::vector<std::string_view> values_to_delete;
  for (const auto& value : value_set) {
    auto& current_value_state = (*existing_value_set)[value];
    if (current_value_state.last_logical_commit_time >= logical_commit_time) {
      // No need to delete
      continue;
    }
    // Add a value that represents a deleted value, or mark the existing value
    // deleted. We need to add the value in deleted state to the map to avoid
    // late arriving update with smaller logical commit time
    // inserting the same value
    current_value_state.last_logical_commit_time = logical_commit_time;
    current_value_state.is_deleted = true;
    values_to_delete.push_back(value);
  }
  if (!values_to_delete.empty()) {
    // Release key lock before locking the map to avoid potential deadlock
    // caused by cycle in the ordering of lock acquisitions
    key_lock.reset();
    absl::MutexLock lock_map(&set_map_mutex_);
    for (const std::string_view value : values_to_delete) {
      deleted_set_nodes_map_[prefix][logical_commit_time][key].emplace(value);
    }
  }
}

void KeyValueCache::DeleteValuesInSet(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    std::string_view key, absl::Span<uint32_t> value_set,
    int64_t logical_commit_time, std::string_view prefix) {
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext,
                              kDeleteUInt32ValueSetLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  if (auto prefix_max_time_node =
          uint32_sets_max_cleanup_commit_time_map_.CGet(prefix);
      prefix_max_time_node.is_present() &&
      logical_commit_time <= *prefix_max_time_node.value()) {
    return;  // Skip old deletes.
  }
  {
    auto cached_set_node = uint32_sets_map_.Get(key);
    if (!cached_set_node.is_present()) {
      auto result = uint32_sets_map_.PutIfAbsent(key, UInt32ValueSet());
      cached_set_node = std::move(result.first);
    }
    cached_set_node.value()->Remove(value_set, logical_commit_time);
  }
  {
    // Mark set as having deleted elements.
    auto prefix_deleted_sets_node = deleted_uint32_sets_map_.Get(prefix);
    if (!prefix_deleted_sets_node.is_present()) {
      auto result = deleted_uint32_sets_map_.PutIfAbsent(
          prefix, absl::btree_map<int64_t, absl::flat_hash_set<std::string>>());
      prefix_deleted_sets_node = std::move(result.first);
    }
    auto* commit_time_sets =
        &(*prefix_deleted_sets_node.value())[logical_commit_time];
    commit_time_sets->insert(std::string(key));
  }
}

void KeyValueCache::RemoveDeletedKeys(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    int64_t logical_commit_time, std::string_view prefix) {
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext,
                              kRemoveDeletedKeyLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  CleanUpKeyValueMap(log_context, logical_commit_time, prefix);
  CleanUpKeyValueSetMap(log_context, logical_commit_time, prefix);
  CleanUpUInt32SetMap(log_context, logical_commit_time, prefix);
}

void KeyValueCache::CleanUpKeyValueMap(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    int64_t logical_commit_time, std::string_view prefix) {
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext,
                              kCleanUpKeyValueMapLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  absl::MutexLock lock(&mutex_);
  if (max_cleanup_logical_commit_time_map_[prefix] < logical_commit_time) {
    max_cleanup_logical_commit_time_map_[prefix] = logical_commit_time;
  }
  auto deleted_nodes_per_prefix = deleted_nodes_map_.find(prefix);
  if (deleted_nodes_per_prefix == deleted_nodes_map_.end()) {
    return;
  }
  auto it = deleted_nodes_per_prefix->second.begin();

  while (it != deleted_nodes_per_prefix->second.end()) {
    if (it->first > logical_commit_time) {
      break;
    }

    // should always have this, but checking just in case
    auto key_iter = map_.find(it->second);
    if (key_iter != map_.end() && key_iter->second.value == nullptr &&
        key_iter->second.last_logical_commit_time <= logical_commit_time) {
      map_.erase(key_iter);
    }

    ++it;
  }
  deleted_nodes_per_prefix->second.erase(
      deleted_nodes_per_prefix->second.begin(), it);
  if (deleted_nodes_per_prefix->second.empty()) {
    deleted_nodes_map_.erase(prefix);
  }
}

void KeyValueCache::CleanUpKeyValueSetMap(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    int64_t logical_commit_time, std::string_view prefix) {
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext,
                              kCleanUpKeyValueSetMapLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  absl::MutexLock lock_set_map(&set_map_mutex_);
  if (set_cache_max_cleanup_logical_commit_time_[prefix] <
      logical_commit_time) {
    set_cache_max_cleanup_logical_commit_time_[prefix] = logical_commit_time;
  }
  auto deleted_nodes_per_prefix = deleted_set_nodes_map_.find(prefix);
  if (deleted_nodes_per_prefix == deleted_set_nodes_map_.end()) {
    return;
  }
  auto delete_itr = deleted_nodes_per_prefix->second.begin();
  while (delete_itr != deleted_nodes_per_prefix->second.end()) {
    if (delete_itr->first > logical_commit_time) {
      break;
    }
    for (const auto& [key, values] : delete_itr->second) {
      if (auto key_itr = key_to_value_set_map_.find(key);
          key_itr != key_to_value_set_map_.end()) {
        {
          absl::MutexLock key_lock(&key_itr->second->first);
          for (const auto& v_to_delete : values) {
            auto existing_value_itr = key_itr->second->second.find(v_to_delete);
            if (existing_value_itr != key_itr->second->second.end() &&
                existing_value_itr->second.is_deleted &&
                existing_value_itr->second.last_logical_commit_time <=
                    logical_commit_time) {
              // Delete the existing value that is marked deleted from set
              key_itr->second->second.erase(existing_value_itr);
            }
          }
        }
        if (key_itr->second->second.empty()) {
          // If the value set is empty, erase the key-value_set from cache map
          key_to_value_set_map_.erase(key);
        }
      }
    }
    ++delete_itr;
  }
  deleted_nodes_per_prefix->second.erase(
      deleted_nodes_per_prefix->second.begin(), delete_itr);
  if (deleted_nodes_per_prefix->second.empty()) {
    deleted_set_nodes_map_.erase(prefix);
  }
}

void KeyValueCache::CleanUpUInt32SetMap(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    int64_t logical_commit_time, std::string_view prefix) {
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext,
                              kCleanUpUInt32SetMapLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  {
    if (auto max_cleanup_time_node =
            uint32_sets_max_cleanup_commit_time_map_.PutIfAbsent(
                prefix, logical_commit_time);
        *max_cleanup_time_node.first.value() < logical_commit_time) {
      *max_cleanup_time_node.first.value() = logical_commit_time;
    }
  }
  absl::flat_hash_set<std::string> cleanup_sets;
  {
    auto prefix_deleted_sets_node = deleted_uint32_sets_map_.Get(prefix);
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
      if (auto set_node = uint32_sets_map_.Get(set); set_node.is_present()) {
        set_node.value()->Cleanup(logical_commit_time);
      }
    }
  }
}

void KeyValueCache::LogCacheAccessMetrics(
    const RequestContext& request_context,
    std::string_view cache_access_event) const {
  LogIfError(
      request_context.GetInternalLookupMetricsContext()
          .AccumulateMetric<kCacheAccessEventCount>(1, cache_access_event));
}

std::unique_ptr<Cache> KeyValueCache::Create() {
  return absl::WrapUnique(new KeyValueCache());
}
}  // namespace kv_server
