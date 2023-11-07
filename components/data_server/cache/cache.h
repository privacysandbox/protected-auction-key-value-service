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
#include "absl/container/flat_hash_set.h"
#include "components/data_server/cache/get_key_value_set_result.h"

namespace kv_server {

// Interface for in-memory datastore.
// One cache object is only for keys in one namespace.
class Cache {
 public:
  virtual ~Cache() = default;

  // Looks up and returns key-value pairs for the given keys.
  virtual absl::flat_hash_map<std::string, std::string> GetKeyValuePairs(
      const absl::flat_hash_set<std::string_view>& key_list) const = 0;

  // Looks up and returns key-value set result for the given key set.
  virtual std::unique_ptr<GetKeyValueSetResult> GetKeyValueSet(
      const absl::flat_hash_set<std::string_view>& key_set) const = 0;

  // Inserts or updates the key with the new value.
  virtual void UpdateKeyValue(std::string_view key, std::string_view value,
                              int64_t logical_commit_time) = 0;

  // Inserts or updates values in the set for a given key, if a value exists,
  // updates its timestamp to the latest logical commit time.
  virtual void UpdateKeyValueSet(std::string_view key,
                                 absl::Span<std::string_view> value_set,
                                 int64_t logical_commit_time) = 0;

  // Deletes a particular (key, value) pair.
  virtual void DeleteKey(std::string_view key, int64_t logical_commit_time) = 0;

  // Deletes values in the set for a given key. The deletion, this object
  // still exist and is marked "deleted", in case there are
  // late-arriving updates to this value.
  virtual void DeleteValuesInSet(std::string_view key,
                                 absl::Span<std::string_view> value_set,
                                 int64_t logical_commit_time) = 0;

  // Removes the values that were deleted before the specified
  // logical_commit_time.
  virtual void RemoveDeletedKeys(int64_t logical_commit_time) = 0;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_CACHE_CACHE_H_
