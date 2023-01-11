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

#ifndef PUBLIC_DATA_LOADING_RECORDS_UTILS_H_
#define PUBLIC_DATA_LOADING_RECORDS_UTILS_H_

#include <string_view>
#include <utility>

#include "public/data_loading/data_loading_generated.h"

namespace kv_server {

std::string_view ToStringView(const flatbuffers::FlatBufferBuilder& fb_buffer);

struct DeltaFileRecordStruct {
  kv_server::DeltaMutationType mutation_type;

  int64_t logical_commit_time;
  std::string_view key;
  std::string_view subkey;
  std::string_view value;

  flatbuffers::FlatBufferBuilder ToFlatBuffer() const;
};

bool operator==(const DeltaFileRecordStruct& lhs_record,
                const DeltaFileRecordStruct& rhs_record);

bool operator!=(const DeltaFileRecordStruct& lhs_record,
                const DeltaFileRecordStruct& rhs_record);

// A `DeltaFileRecordStructKey` defines a hashable object that adheres to
// the `absl::Hash` framework - https://abseil.io/docs/cpp/guides/hash#abslhash
// which can be used as a key in associative containers to store
// `DeltaFileRecordStruct` records.
//
// (1) To use with std lib associative containers, pass in
// `absl::Hash<DeltaFileRecordStructKey>` as the hash functor, e.g.:
// ```
//   std::unordered_map<DeltaFileRecordStructKey, DeltaFileRecordStruct,
//                      absl::Hash<DeltaFileRecordStructKey>> map;
// ```
// (2) To manually generate hash values for `DeltaFileRecordStruct` records, use
// the following approach:
// ```
//   DeltaFileRecordStruct record{.key = ..., .subkey = ..., ...};
//   DeltaFileRecordStructKey record_key{.key = record.key, .subkey =
//   record.subkey};
//   size_t hash_value = absl::HashOf(record_key);
// ```

struct DeltaFileRecordStructKey {
  std::string_view key = "";
  std::string_view subkey = "";

  template <typename H>
  friend H AbslHashValue(H h, const DeltaFileRecordStructKey& key) {
    return H::combine(std::move(h), key.key, key.subkey);
  }
};

bool operator==(const DeltaFileRecordStructKey& lhs_key,
                const DeltaFileRecordStructKey& rhs_key);
}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_RECORDS_UTILS_H_
