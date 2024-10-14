/*
 * Copyright 2024 Google LLC
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

#include <string_view>

#include "absl/container/flat_hash_set.h"

#include "roaring.hh"
#include "roaring64map.hh"

namespace kv_server {

template <typename T>
struct SetTypeConverter;

template <>
struct SetTypeConverter<absl::flat_hash_set<std::string_view>> {
  using type = std::string_view;
};

template <>
struct SetTypeConverter<roaring::Roaring> {
  using type = uint32_t;
};

template <>
struct SetTypeConverter<roaring::Roaring64Map> {
  using type = uint64_t;
};

template <typename T>
using ConvertedSetType = typename SetTypeConverter<T>::type;

}  // namespace kv_server
