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

#ifndef COMPONENTS_DATA_SERVER_CACHE_GET_KEY_VALUE_SET_RESULT_H_
#define COMPONENTS_DATA_SERVER_CACHE_GET_KEY_VALUE_SET_RESULT_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "components/container/thread_safe_hash_map.h"
#include "components/data_server/cache/uint32_value_set.h"

namespace kv_server {
// Class that holds the data retrieved from cache lookup and read locks for
// the lookup keys
class GetKeyValueSetResult {
 public:
  virtual ~GetKeyValueSetResult() = default;

  // Looks up and returns key-value set result for the given key set.
  virtual absl::flat_hash_set<std::string_view> GetValueSet(
      std::string_view key) const = 0;
  virtual const UInt32ValueSet* GetUInt32ValueSet(
      std::string_view key) const = 0;

 private:
  // Adds key, value_set to the result data map, mantains the lock on `key`
  // until this object goes out of scope.
  virtual void AddKeyValueSet(
      std::string_view key, absl::flat_hash_set<std::string_view> value_set,
      std::unique_ptr<absl::ReaderMutexLock> key_lock) = 0;
  virtual void AddUInt32ValueSet(
      std::string_view key,
      ThreadSafeHashMap<std::string, UInt32ValueSet>::ConstLockedNodePtr
          value_set_node) = 0;

  static std::unique_ptr<GetKeyValueSetResult> Create();

  friend class KeyValueCache;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_CACHE_GET_KEY_VALUE_SET_RESULT_H_
