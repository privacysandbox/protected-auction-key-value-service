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

#include "components/query/sets.h"

#include <utility>

namespace kv_server {

template <>
absl::flat_hash_set<std::string_view> Union(
    absl::flat_hash_set<std::string_view>&& left,
    absl::flat_hash_set<std::string_view>&& right) {
  auto& small = left.size() <= right.size() ? left : right;
  auto& big = left.size() <= right.size() ? right : left;
  big.insert(small.begin(), small.end());
  return std::move(big);
}

template <>
absl::flat_hash_set<std::string_view> Intersection(
    absl::flat_hash_set<std::string_view>&& left,
    absl::flat_hash_set<std::string_view>&& right) {
  auto& small = left.size() <= right.size() ? left : right;
  const auto& big = left.size() <= right.size() ? right : left;
  // Traverse the smaller set removing what is not in both.
  absl::erase_if(small, [&big](const std::string_view& elem) {
    return !big.contains(elem);
  });
  return std::move(small);
}

template <>
absl::flat_hash_set<std::string_view> Difference(
    absl::flat_hash_set<std::string_view>&& left,
    absl::flat_hash_set<std::string_view>&& right) {
  // Remove all elements in right from left.
  for (const auto& element : right) {
    left.erase(element);
  }
  return std::move(left);
}

template <>
roaring::Roaring Union(roaring::Roaring&& left, roaring::Roaring&& right) {
  return left | right;
}

template <>
roaring::Roaring Intersection(roaring::Roaring&& left,
                              roaring::Roaring&& right) {
  return left & right;
}

template <>
roaring::Roaring Difference(roaring::Roaring&& left, roaring::Roaring&& right) {
  return left - right;
}

}  // namespace kv_server
