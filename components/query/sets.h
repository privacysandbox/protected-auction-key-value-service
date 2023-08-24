/*
 * Copyright 2023 Google LLC
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

#ifndef COMPONENTS_QUERY_SETS_H_
#define COMPONENTS_QUERY_SETS_H_

#include <utility>

#include "absl/container/flat_hash_set.h"

namespace kv_server {
template <typename T>
absl::flat_hash_set<T> Union(absl::flat_hash_set<T>&& left,
                             absl::flat_hash_set<T>&& right) {
  auto& small = left.size() <= right.size() ? left : right;
  auto& big = left.size() <= right.size() ? right : left;
  big.insert(small.begin(), small.end());
  return std::move(big);
}

template <typename T>
absl::flat_hash_set<T> Intersection(absl::flat_hash_set<T>&& left,
                                    absl::flat_hash_set<T>&& right) {
  auto& small = left.size() <= right.size() ? left : right;
  const auto& big = left.size() <= right.size() ? right : left;
  // Traverse the smaller set removing what is not in both.
  absl::erase_if(small, [&big](const T& elem) { return !big.contains(elem); });
  return std::move(small);
}

template <typename T>
absl::flat_hash_set<T> Difference(absl::flat_hash_set<T>&& left,
                                  absl::flat_hash_set<T>&& right) {
  // Remove all elements in right from left.
  for (const auto& element : right) {
    left.erase(element);
  }
  return std::move(left);
}

}  // namespace kv_server
#endif  // COMPONENTS_QUERY_SETS_H_
