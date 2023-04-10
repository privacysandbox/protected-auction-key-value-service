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
#include <utility>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "components/data_server/cache/cache.h"
#include "glog/logging.h"
#include "public/base_types.pb.h"

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
      kv_pairs.insert_or_assign(key, *(key_iter->second.value));
    }
  }
  return kv_pairs;
}

// Replaces the current key-value entry with the new key-value entry.
void KeyValueCache::UpdateKeyValue(std::string_view key, std::string_view value,
                                   int64_t logical_commit_time) {
  absl::MutexLock lock(&mutex_);

  if (logical_commit_time <= max_cleanup_logical_commit_time_) {
    return;
  }

  const auto key_iter = map_.find(key);

  if (key_iter != map_.end() &&
      key_iter->second.last_logical_commit_time >= logical_commit_time) {
    return;
  }

  if (key_iter != map_.end() &&
      key_iter->second.last_logical_commit_time < logical_commit_time &&
      key_iter->second.value == nullptr) {
    // should always have this, but checking just in case
    auto dl_key_iter = deleted_nodes_.find(logical_commit_time);
    if (dl_key_iter != deleted_nodes_.end() && dl_key_iter->second == key) {
      deleted_nodes_.erase(dl_key_iter);
    }
  }

  map_.insert_or_assign(key, {.value = std::make_unique<std::string>(value),
                              .last_logical_commit_time = logical_commit_time});
}

void KeyValueCache::DeleteKey(std::string_view key,
                              int64_t logical_commit_time) {
  absl::MutexLock lock(&mutex_);
  const auto key_iter = map_.find(key);
  if (key_iter != map_.end() &&
      key_iter->second.last_logical_commit_time < logical_commit_time) {
    map_.insert_or_assign(
        key,
        {.value = nullptr, .last_logical_commit_time = logical_commit_time});

    auto result = deleted_nodes_.insert_or_assign(logical_commit_time, key);
    if (!result.second) {
      // assignment took place -- this should never happen as the
      // logical_commit_time is globally unique
      LOG(ERROR) << "Assignment happened for logical commit time "
                 << logical_commit_time << " key " << key;
    }
  }
}

void KeyValueCache::RemoveDeletedKeys(int64_t logical_commit_time) {
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

  max_cleanup_logical_commit_time_ =
      std::max(max_cleanup_logical_commit_time_, logical_commit_time);

  deleted_nodes_.erase(deleted_nodes_.begin(), it);
}

std::unique_ptr<Cache> KeyValueCache::Create() {
  return std::make_unique<KeyValueCache>();
}
}  // namespace kv_server
