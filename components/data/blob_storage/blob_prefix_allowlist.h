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

#ifndef COMPONENTS_DATA_BLOB_STORAGE_BLOB_PREFIX_ALLOWLIST_H_
#define COMPONENTS_DATA_BLOB_STORAGE_BLOB_PREFIX_ALLOWLIST_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"

namespace kv_server {

// List of blob prefixes that are allowlisted for data loading. Blobs with
// prefix not included in this are ignored.
class BlobPrefixAllowlist {
 public:
  struct BlobName {
    std::string prefix;
    std::string key;
  };

  explicit BlobPrefixAllowlist(std::string_view allowed_prefixes);

  // Returns true if `prefix` is allowlisted, meaning that blobs with this
  // prefix are eligible for data loading.
  [[nodiscard]] bool Contains(std::string_view prefix) const;
  // Returns true if `blob_name`'s is allowlisted, meaning that  the blob is
  // eligible for data loading.
  [[nodiscard]] bool ContainsBlobPrefix(std::string_view blob_name) const;

 private:
  absl::flat_hash_set<std::string> allowed_prefixes_;
};

// Parses a `blob_name` into it's corresponding name parts.
//
// For example:
// (1) blob_name="prefix1/DELTA_1705430864435450" => {.prefix="prefix1",
//  .key="DELTA_1705430864435450"}
// (2) blob_name="DELTA_1705430864435450" => {.prefix="",
// .key="DELTA_1705430864435450"}
BlobPrefixAllowlist::BlobName ParseBlobName(std::string_view blob_name);

}  // namespace kv_server

#endif  // COMPONENTS_DATA_BLOB_STORAGE_BLOB_PREFIX_ALLOWLIST_H_
