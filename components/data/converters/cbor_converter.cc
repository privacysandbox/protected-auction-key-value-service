// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/data/converters/cbor_converter.h"

#include <utility>

#include "components/data/converters/cbor_converter_utils.h"
#include "components/data/converters/scoped_cbor.h"
#include "public/query/v2/get_values_v2.pb.h"
#include "src/util/status_macro/status_macros.h"

#include "cbor.h"

namespace kv_server {

inline constexpr char kCompressionGroups[] = "compressionGroups";
inline constexpr char kCompressionGroupId[] = "compressionGroupId";
inline constexpr char kTtlMs[] = "ttlMs";
inline constexpr char kContent[] = "content";

absl::StatusOr<cbor_item_t*> EncodeCompressionGroup(
    v2::CompressionGroup& compression_group) {
  const int compressionGroupKeysNumber = 3;
  auto* cbor_internal = cbor_new_definite_map(compressionGroupKeysNumber);
  PS_RETURN_IF_ERROR(CborSerializeUInt(kCompressionGroupId,
                                       compression_group.compression_group_id(),
                                       *cbor_internal));
  if (compression_group.has_ttl_ms()) {
    PS_RETURN_IF_ERROR(
        CborSerializeUInt(kTtlMs, compression_group.ttl_ms(), *cbor_internal));
  }

  PS_RETURN_IF_ERROR(CborSerializeString(
      kContent, std::move(compression_group.content()), *cbor_internal));
  return cbor_internal;
}

absl::StatusOr<cbor_item_t*> EncodeCompressionGroups(
    google::protobuf::RepeatedPtrField<v2::CompressionGroup>&
        compression_groups) {
  cbor_item_t* serialized_compression_groups =
      cbor_new_definite_array(compression_groups.size());
  for (auto& compression_group : compression_groups) {
    PS_ASSIGN_OR_RETURN(auto* serialized_compression_group,
                        EncodeCompressionGroup(compression_group));
    if (!cbor_array_push(serialized_compression_groups,
                         cbor_move(serialized_compression_group))) {
      return absl::InternalError(
          absl::StrCat("Failed to serialize ", kCompressionGroups, " to CBOR"));
    }
  }

  return serialized_compression_groups;
}

absl::StatusOr<std::string> V2GetValuesResponseCborEncode(
    v2::GetValuesResponse& response) {
  const int getValuesResponseKeysNumber = 1;
  ScopedCbor root(cbor_new_definite_map(getValuesResponseKeysNumber));
  PS_ASSIGN_OR_RETURN(
      auto* compression_groups,
      EncodeCompressionGroups(*(response.mutable_compression_groups())));
  struct cbor_pair serialized_kCompressionGroups = {
      .key = cbor_move(cbor_build_stringn(kCompressionGroups,
                                          sizeof(kCompressionGroups) - 1)),
      .value = compression_groups,
  };
  auto* cbor_internal = root.get();
  if (!cbor_map_add(cbor_internal, serialized_kCompressionGroups)) {
    return absl::InternalError(
        absl::StrCat("Failed to serialize ", kCompressionGroups, " to CBOR"));
  }
  return GetCborSerializedResult(*cbor_internal);
}

}  // namespace kv_server
