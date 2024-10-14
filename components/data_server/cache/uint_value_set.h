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

#ifndef COMPONENTS_DATA_SERVER_CACHE_UINT_VALUE_SET_H_
#define COMPONENTS_DATA_SERVER_CACHE_UINT_VALUE_SET_H_

#include <memory>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

#include "roaring.hh"
#include "roaring64map.hh"

namespace kv_server {

// Stores a set of unsigned int values associated with a `logical_commit_time`.
// The `logical_commit_time` is used to support out of order set mutations,
// i.e., calling `Remove({1, 2, 3}, 5)` and then `Add({1, 2, 3}, 3)` will result
// in an empty set.
//
// The values in the set are also projected to `roaring::Roaring` bitset which
// can be used for efficient set operations such as union, intersection, .e.t.c.
template <typename ValueType, typename BitsetType>
class UIntValueSet {
 public:
  using value_type = ValueType;
  using bitset_type = BitsetType;

  // Returns values not marked as removed from the set.
  absl::flat_hash_set<ValueType> GetValues() const;
  // Returns values not marked as removed from the set as a bitset.
  const BitsetType& GetValuesBitSet() const;
  // Returns values marked as removed from the set.
  absl::flat_hash_set<ValueType> GetRemovedValues() const;

  // Adds values associated with `logical_commit_time` to the set. If a value
  // with the same or greater `logical_commit_time` already exists in the set,
  // then this is a noop.
  void Add(absl::Span<ValueType> values, int64_t logical_commit_time);
  // Marks values associated with `logical_commit_time` as removed from the set.
  // If a value with the same or greater `logical_commit_time` already exists in
  // the set, then this is a noop.
  void Remove(absl::Span<ValueType> values, int64_t logical_commit_time);
  // Cleans up space occupied by values (including value metadata) matching the
  // condition `logical_commit_time` <= `cutoff_logical_commit_time` and are
  // marked as removed.
  void Cleanup(int64_t cutoff_logical_commit_time);

 private:
  struct ValueMetadata {
    int64_t logical_commit_time;
    bool is_deleted;
  };

  void AddOrRemove(absl::Span<ValueType> values, int64_t logical_commit_time,
                   bool is_deleted);

  BitsetType values_bitset_;
  absl::flat_hash_map<ValueType, ValueMetadata> values_metadata_;
  absl::btree_map<int64_t, absl::flat_hash_set<ValueType>> deleted_values_;
};

// Define specialized aliases for 32 and 64 bit unsigned int sets.
using UInt32ValueSet = UIntValueSet<uint32_t, roaring::Roaring>;
using UInt64ValueSet = UIntValueSet<uint64_t, roaring::Roaring64Map>;

template <typename ValueType, typename BitsetType>
absl::flat_hash_set<ValueType> UIntValueSet<ValueType, BitsetType>::GetValues()
    const {
  absl::flat_hash_set<ValueType> values;
  values.reserve(values_bitset_.cardinality());
  for (const auto& [value, metadata] : values_metadata_) {
    if (!metadata.is_deleted) {
      values.insert(value);
    }
  }
  return values;
}

template <typename ValueType, typename BitsetType>
const BitsetType& UIntValueSet<ValueType, BitsetType>::GetValuesBitSet() const {
  return values_bitset_;
}

template <typename ValueType, typename BitsetType>
absl::flat_hash_set<ValueType>
UIntValueSet<ValueType, BitsetType>::GetRemovedValues() const {
  absl::flat_hash_set<ValueType> removed_values;
  for (const auto& [_, values] : deleted_values_) {
    for (auto value : values) {
      removed_values.insert(value);
    }
  }
  return removed_values;
}

template <typename ValueType, typename BitsetType>
void UIntValueSet<ValueType, BitsetType>::AddOrRemove(
    absl::Span<ValueType> values, int64_t logical_commit_time,
    bool is_deleted) {
  for (auto value : values) {
    auto* metadata = &values_metadata_[value];
    if (metadata->logical_commit_time >= logical_commit_time) {
      continue;
    }
    metadata->logical_commit_time = logical_commit_time;
    metadata->is_deleted = is_deleted;
    if (is_deleted) {
      values_bitset_.remove(value);
      deleted_values_[logical_commit_time].insert(value);
    } else {
      values_bitset_.add(value);
      deleted_values_[logical_commit_time].erase(value);
    }
  }
  values_bitset_.runOptimize();
}

template <typename ValueType, typename BitsetType>
void UIntValueSet<ValueType, BitsetType>::Add(absl::Span<ValueType> values,
                                              int64_t logical_commit_time) {
  AddOrRemove(values, logical_commit_time, /*is_deleted=*/false);
}

template <typename ValueType, typename BitsetType>
void UIntValueSet<ValueType, BitsetType>::Remove(absl::Span<ValueType> values,
                                                 int64_t logical_commit_time) {
  AddOrRemove(values, logical_commit_time, /*is_deleted=*/true);
}

template <typename ValueType, typename BitsetType>
void UIntValueSet<ValueType, BitsetType>::Cleanup(
    int64_t cutoff_logical_commit_time) {
  for (const auto& [logical_commit_time, values] : deleted_values_) {
    if (logical_commit_time > cutoff_logical_commit_time) {
      break;
    }
    for (auto value : values) {
      values_metadata_.erase(value);
    }
  }
  deleted_values_.erase(
      deleted_values_.begin(),
      deleted_values_.upper_bound(cutoff_logical_commit_time));
}

template <typename ValueType, typename BitsetType>
absl::flat_hash_set<ValueType> BitSetToUintSet(const BitsetType& bitset) {
  auto num_values = bitset.cardinality();
  auto data = std::make_unique<ValueType[]>(num_values);
  if constexpr (std::is_same_v<BitsetType, roaring::Roaring>) {
    bitset.toUint32Array(data.get());
  }
  if constexpr (std::is_same_v<BitsetType, roaring::Roaring64Map>) {
    bitset.toUint64Array(data.get());
  }
  return absl::flat_hash_set<ValueType>(data.get(), data.get() + num_values);
}

absl::flat_hash_set<uint32_t> BitSetToUint32Set(const roaring::Roaring& bitset);
absl::flat_hash_set<uint64_t> BitSetToUint64Set(
    const roaring::Roaring64Map& bitset);

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_CACHE_UINT_VALUE_SET_H_
