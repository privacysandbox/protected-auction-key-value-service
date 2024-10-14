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

template <typename SetType>
SetType Union(SetType&& left, SetType&& right) {
  return std::forward<SetType>(left) | std::forward<SetType>(right);
}

template <typename SetType>
SetType Intersection(SetType&& left, SetType&& right) {
  return std::forward<SetType>(left) & std::forward<SetType>(right);
}

template <typename SetType>
SetType Difference(SetType&& left, SetType&& right) {
  return std::forward<SetType>(left) - std::forward<SetType>(right);
}

template <>
absl::flat_hash_set<std::string_view> Union(
    absl::flat_hash_set<std::string_view>&& left,
    absl::flat_hash_set<std::string_view>&& right);

template <>
absl::flat_hash_set<std::string_view> Intersection(
    absl::flat_hash_set<std::string_view>&& left,
    absl::flat_hash_set<std::string_view>&& right);

template <>
absl::flat_hash_set<std::string_view> Difference(
    absl::flat_hash_set<std::string_view>&& left,
    absl::flat_hash_set<std::string_view>&& right);

}  // namespace kv_server
#endif  // COMPONENTS_QUERY_SETS_H_
