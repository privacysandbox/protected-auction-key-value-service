/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "components/data_server/cache/get_key_value_set_result.h"

namespace kv_server {
namespace {

// Class that holds the data retrieved from cache lookup and read locks for
// the lookup keys
class GetKeyValueSetResultImpl : public GetKeyValueSetResult {
 public:
  GetKeyValueSetResultImpl() {}
  // Looks up the key in the data map and returns value set. If the value_set
  // for the key is missing, returns empty set.
  absl::flat_hash_set<std::string_view> GetValueSet(
      std::string_view key) const override {
    static const absl::flat_hash_set<std::string_view>* kEmptySet =
        new absl::flat_hash_set<std::string_view>();
    auto key_itr = data_map_.find(key);
    return key_itr == data_map_.end() ? *kEmptySet : key_itr->second;
  }
  GetKeyValueSetResultImpl(const GetKeyValueSetResultImpl&) = delete;
  GetKeyValueSetResultImpl& operator=(const GetKeyValueSetResultImpl&) = delete;
  GetKeyValueSetResultImpl(GetKeyValueSetResultImpl&& other) = default;
  GetKeyValueSetResultImpl& operator=(GetKeyValueSetResultImpl&& other) =
      default;

 private:
  std::vector<std::unique_ptr<absl::ReaderMutexLock>> read_locks_;
  absl::flat_hash_map<std::string_view, absl::flat_hash_set<std::string_view>>
      data_map_;

  // Adds key, value_set to the result data map, creates a read lock for
  // the key mutex
  void AddKeyValueSet(
      absl::Mutex& key_mutex, std::string_view key,
      const absl::flat_hash_set<std::string_view>& value_set) override {
    read_locks_.emplace_back(std::move(new absl::ReaderMutexLock(&key_mutex)));
    data_map_.emplace(key, value_set);
  }
};
}  // namespace

std::unique_ptr<GetKeyValueSetResult> GetKeyValueSetResult::Create() {
  return std::make_unique<GetKeyValueSetResultImpl>();
}

}  // namespace kv_server
