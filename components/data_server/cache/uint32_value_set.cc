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

#include "components/data_server/cache/uint32_value_set.h"

#include <memory>

namespace kv_server {

absl::flat_hash_set<uint32_t> UInt32ValueSet::GetValues() const {
  absl::flat_hash_set<uint32_t> values;
  values.reserve(values_bitset_.cardinality());
  for (const auto& [value, metadata] : values_metadata_) {
    if (!metadata.is_deleted) {
      values.insert(value);
    }
  }
  return values;
}

const roaring::Roaring& UInt32ValueSet::GetValuesBitSet() const {
  return values_bitset_;
}

absl::flat_hash_set<uint32_t> UInt32ValueSet::GetRemovedValues() const {
  absl::flat_hash_set<uint32_t> removed_values;
  for (const auto& [_, values] : deleted_values_) {
    for (auto value : values) {
      removed_values.insert(value);
    }
  }
  return removed_values;
}

void UInt32ValueSet::AddOrRemove(absl::Span<uint32_t> values,
                                 int64_t logical_commit_time, bool is_deleted) {
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

void UInt32ValueSet::Add(absl::Span<uint32_t> values,
                         int64_t logical_commit_time) {
  AddOrRemove(values, logical_commit_time, /*is_deleted=*/false);
}

void UInt32ValueSet::Remove(absl::Span<uint32_t> values,
                            int64_t logical_commit_time) {
  AddOrRemove(values, logical_commit_time, /*is_deleted=*/true);
}

void UInt32ValueSet::Cleanup(int64_t cutoff_logical_commit_time) {
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

absl::flat_hash_set<uint32_t> BitSetToUint32Set(
    const roaring::Roaring& bitset) {
  auto num_values = bitset.cardinality();
  auto data = std::make_unique<uint32_t[]>(num_values);
  bitset.toUint32Array(data.get());
  return absl::flat_hash_set<uint32_t>(data.get(), data.get() + num_values);
}

}  // namespace kv_server
