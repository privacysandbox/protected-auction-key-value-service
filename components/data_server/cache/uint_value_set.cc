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

#include "components/data_server/cache/uint_value_set.h"

namespace kv_server {

absl::flat_hash_set<uint32_t> BitSetToUint32Set(
    const roaring::Roaring& bitset) {
  return BitSetToUintSet<uint32_t, roaring::Roaring>(bitset);
}

absl::flat_hash_set<uint64_t> BitSetToUint64Set(
    const roaring::Roaring64Map& bitset) {
  return BitSetToUintSet<uint64_t, roaring::Roaring64Map>(bitset);
}

}  // namespace kv_server
