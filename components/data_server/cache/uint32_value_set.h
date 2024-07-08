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

#ifndef COMPONENTS_DATA_SERVER_CACHE_UINT32_VALUE_SET_H_
#define COMPONENTS_DATA_SERVER_CACHE_UINT32_VALUE_SET_H_

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

#include "roaring.hh"

namespace kv_server {

// Stores a set of `uint32_t` values associated with a `logical_commit_time`.
// The `logical_commit_time` is used to support out of order set mutations,
// i.e., calling `Remove({1, 2, 3}, 5)` and then `Add({1, 2, 3}, 3)` will result
// in an empty set.
//
// The values in the set are also projected to `roaring::Roaring` bitset which
// can be used for efficient set operations such as union, intersection, .e.t.c.
class UInt32ValueSet {
 public:
  // Returns values not marked as removed from the set.
  absl::flat_hash_set<uint32_t> GetValues() const;
  // Returns values not marked as removed from the set as a bitset.
  const roaring::Roaring& GetValuesBitSet() const;
  // Returns values marked as removed from the set.
  absl::flat_hash_set<uint32_t> GetRemovedValues() const;

  // Adds values associated with `logical_commit_time` to the set. If a value
  // with the same or greater `logical_commit_time` already exists in the set,
  // then this is a noop.
  void Add(absl::Span<uint32_t> values, int64_t logical_commit_time);
  // Marks values associated with `logical_commit_time` as removed from the set.
  // If a value with the same or greater `logical_commit_time` already exists in
  // the set, then this is a noop.
  void Remove(absl::Span<uint32_t> values, int64_t logical_commit_time);
  // Cleans up space occupied by values (including value metadata) matching the
  // condition `logical_commit_time` <= `cutoff_logical_commit_time` and are
  // marked as removed.
  void Cleanup(int64_t cutoff_logical_commit_time);

 private:
  struct ValueMetadata {
    int64_t logical_commit_time;
    bool is_deleted;
  };

  void AddOrRemove(absl::Span<uint32_t> values, int64_t logical_commit_time,
                   bool is_deleted);

  roaring::Roaring values_bitset_;
  absl::flat_hash_map<uint32_t, ValueMetadata> values_metadata_;
  absl::btree_map<int64_t, absl::flat_hash_set<uint32_t>> deleted_values_;
};

absl::flat_hash_set<uint32_t> BitSetToUint32Set(const roaring::Roaring& bitset);

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_CACHE_UINT32_VALUE_SET_H_
