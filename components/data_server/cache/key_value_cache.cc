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
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/get_key_value_set_result.h"
#include "glog/logging.h"

namespace kv_server {

absl::flat_hash_map<std::string, std::string> KeyValueCache::GetKeyValuePairs(
    const std::vector<std::string_view>& key_list) const {
  absl::flat_hash_map<std::string, std::string> kv_pairs;
  absl::ReaderMutexLock lock(&mutex_);
  for (std::string_view key : key_list) {
    const auto key_iter = map_.find(key);
    if (key_iter == map_.end() || key_iter->second.value == nullptr) {
      continue;
    } else {
      VLOG(9) << "Get called for " << key
              << ". returning value: " << *(key_iter->second.value);
      kv_pairs.insert_or_assign(key, *(key_iter->second.value));
    }
  }
  return kv_pairs;
}

std::unique_ptr<GetKeyValueSetResult> KeyValueCache::GetKeyValueSet(
    const absl::flat_hash_set<std::string_view>& key_set) const {
  // lock the cache map
  absl::ReaderMutexLock lock(&set_map_mutex_);
  auto result = GetKeyValueSetResult::Create();
  for (const auto& key : key_set) {
    VLOG(8) << "Getting key: " << key;
    const auto key_itr = key_to_value_set_map_.find(key);
    if (key_itr != key_to_value_set_map_.end()) {
      absl::flat_hash_set<std::string_view> value_set;
      for (const auto& v : key_itr->second->second) {
        if (!v.second.is_deleted) {
          value_set.emplace(v.first);
        }
      }
      // Add key value set to the result
      result->AddKeyValueSet(key_itr->second->first, key, std::move(value_set));
    }
  }
  return result;
}

// Replaces the current key-value entry with the new key-value entry.
void KeyValueCache::UpdateKeyValue(std::string_view key, std::string_view value,
                                   int64_t logical_commit_time) {
  VLOG(9) << "Received update for [" << key << "] at " << logical_commit_time
          << ". value will be set to: " << value;
  absl::MutexLock lock(&mutex_);

  if (logical_commit_time <= max_cleanup_logical_commit_time_) {
    VLOG(1) << "Skipping the update as its logical_commit_time: "
            << logical_commit_time << " is older than the current cutoff time:"
            << max_cleanup_logical_commit_time_;

    return;
  }

  const auto key_iter = map_.find(key);

  if (key_iter != map_.end() &&
      key_iter->second.last_logical_commit_time >= logical_commit_time) {
    VLOG(1) << "Skipping the update as its logical_commit_time: "
            << logical_commit_time << " is older than the current value's time:"
            << key_iter->second.last_logical_commit_time;
    return;
  }

  if (key_iter != map_.end() &&
      key_iter->second.last_logical_commit_time < logical_commit_time &&
      key_iter->second.value == nullptr) {
    // should always have this, but checking just in case
    auto dl_key_iter =
        deleted_nodes_.find(key_iter->second.last_logical_commit_time);
    if (dl_key_iter != deleted_nodes_.end() && dl_key_iter->second == key) {
      deleted_nodes_.erase(dl_key_iter);
    }
  }

  map_.insert_or_assign(key, {.value = std::make_unique<std::string>(value),
                              .last_logical_commit_time = logical_commit_time});
}

void KeyValueCache::UpdateKeyValueSet(
    std::string_view key, absl::Span<std::string_view> input_value_set,
    int64_t logical_commit_time) {
  VLOG(9) << "Received update for [" << key << "] at " << logical_commit_time;
  std::unique_ptr<absl::MutexLock> key_lock;
  absl::flat_hash_map<std::string, SetValueMeta>* existing_value_set;
  // The max cleanup time needs to be locked before doing this comparison
  {
    absl::MutexLock lock_map(&set_map_mutex_);

    if (logical_commit_time <= max_cleanup_logical_commit_time_for_set_cache_) {
      VLOG(1) << "Skipping the update as its logical_commit_time: "
              << logical_commit_time
              << " is older than the current cutoff time:"
              << max_cleanup_logical_commit_time_for_set_cache_;
      return;
    } else if (input_value_set.empty()) {
      VLOG(1) << "Skipping the update as it has no value in the set.";
      return;
    }
    auto key_itr = key_to_value_set_map_.find(key);
    if (key_itr == key_to_value_set_map_.end()) {
      VLOG(9) << key << " is a new key. Adding it";
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

void KeyValueCache::DeleteKey(std::string_view key,
                              int64_t logical_commit_time) {
  absl::MutexLock lock(&mutex_);

  if (logical_commit_time <= max_cleanup_logical_commit_time_) {
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

    auto result = deleted_nodes_.emplace(logical_commit_time, key);
  }
}

void KeyValueCache::DeleteValuesInSet(std::string_view key,
                                      absl::Span<std::string_view> value_set,
                                      int64_t logical_commit_time) {
  std::unique_ptr<absl::MutexLock> key_lock;
  absl::flat_hash_map<std::string, SetValueMeta>* existing_value_set;
  // The max cleanup time needs to be locked before doing this comparison
  {
    absl::MutexLock lock_map(&set_map_mutex_);

    if (logical_commit_time <= max_cleanup_logical_commit_time_for_set_cache_ ||
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
        deleted_set_nodes_[logical_commit_time][key].emplace(value);
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
      deleted_set_nodes_[logical_commit_time][key].emplace(value);
    }
  }
}

void KeyValueCache::RemoveDeletedKeys(int64_t logical_commit_time) {
  CleanUpKeyValueMap(logical_commit_time);
  CleanUpKeyValueSetMap(logical_commit_time);
}

void KeyValueCache::CleanUpKeyValueMap(int64_t logical_commit_time) {
  absl::MutexLock lock(&mutex_);
  auto it = deleted_nodes_.begin();

  while (it != deleted_nodes_.end()) {
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
  deleted_nodes_.erase(deleted_nodes_.begin(), it);
  max_cleanup_logical_commit_time_ =
      std::max(max_cleanup_logical_commit_time_, logical_commit_time);
}

void KeyValueCache::CleanUpKeyValueSetMap(int64_t logical_commit_time) {
  absl::MutexLock lock_set_map(&set_map_mutex_);
  auto delete_itr = deleted_set_nodes_.begin();
  while (delete_itr != deleted_set_nodes_.end()) {
    if (delete_itr->first > logical_commit_time) {
      break;
    }
    for (const auto& [key, values] : delete_itr->second) {
      if (auto key_itr = key_to_value_set_map_.find(key);
          key_itr != key_to_value_set_map_.end()) {
        absl::MutexLock(&key_itr->second->first);
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
        if (key_itr->second->second.empty()) {
          // If the value set is empty, erase the key-value_set from cache map
          key_to_value_set_map_.erase(key);
        }
      }
    }
    ++delete_itr;
  }
  deleted_set_nodes_.erase(deleted_set_nodes_.begin(), delete_itr);
  max_cleanup_logical_commit_time_for_set_cache_ = std::max(
      max_cleanup_logical_commit_time_for_set_cache_, logical_commit_time);
}

std::unique_ptr<Cache> KeyValueCache::Create() {
  return std::make_unique<KeyValueCache>();
}
}  // namespace kv_server
