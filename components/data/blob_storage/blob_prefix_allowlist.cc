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

#include "components/data/blob_storage/blob_prefix_allowlist.h"

#include <algorithm>
#include <utility>

#include "absl/strings/str_split.h"

namespace kv_server {
namespace {
constexpr std::string_view kBlobNameDelimiter = "/";
constexpr std::string_view kPrefixListDelimiter = ",";
}  // namespace

BlobPrefixAllowlist::BlobName ParseBlobName(std::string_view blob_name) {
  std::string blob_name_copy(blob_name);
  std::reverse(blob_name_copy.begin(), blob_name_copy.end());
  std::vector<std::string> name_parts = absl::StrSplit(
      blob_name_copy,
      absl::MaxSplits(/*delimiter=*/kBlobNameDelimiter, /*limit=*/1));
  if (name_parts.size() == 0) {
    return BlobPrefixAllowlist::BlobName{};
  }
  for (int64_t part = 0; part < name_parts.size(); part++) {
    std::reverse(name_parts[part].begin(), name_parts[part].end());
  }
  auto prefix = name_parts.size() == 1 ? "" : std::move(name_parts.back());
  return BlobPrefixAllowlist::BlobName{.prefix = std::move(prefix),
                                       .key = std::move(name_parts.front())};
}

BlobPrefixAllowlist::BlobPrefixAllowlist(std::string_view allowed_prefixes) {
  std::vector<std::string> prefixes =
      absl::StrSplit(allowed_prefixes, kPrefixListDelimiter);
  // We always allow reading blobs at the bucket level.
  allowed_prefixes_.insert("");
  allowed_prefixes_.insert(prefixes.begin(), prefixes.end());
}

bool BlobPrefixAllowlist::Contains(std::string_view prefix) {
  return allowed_prefixes_.contains(prefix);
}

bool BlobPrefixAllowlist::ContainsBlobPrefix(std::string_view blob_name) {
  return Contains(ParseBlobName(blob_name).prefix);
}

}  // namespace kv_server
