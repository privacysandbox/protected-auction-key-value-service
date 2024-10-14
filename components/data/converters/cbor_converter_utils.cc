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

#include "components/data/converters/cbor_converter_utils.h"

#include "absl/strings/str_cat.h"

// TODO(b/355941387): move common methods to the common repo
namespace kv_server {
cbor_item_t* cbor_build_uint(uint32_t input) {
  if (input <= 255) {
    return cbor_build_uint8(input);
  } else if (input <= 65535) {
    return cbor_build_uint16(input);
  }
  return cbor_build_uint32(input);
}

absl::Status CborSerializeUInt(absl::string_view key, uint32_t value,
                               cbor_item_t& root) {
  struct cbor_pair kv = {
      .key = cbor_move(cbor_build_stringn(key.data(), key.size())),
      .value = cbor_build_uint(value)};
  if (!cbor_map_add(&root, kv)) {
    return absl::InternalError(
        absl::StrCat("Failed to serialize ", key, " to CBOR"));
  }
  return absl::OkStatus();
}

absl::Status CborSerializeString(absl::string_view key, absl::string_view value,
                                 cbor_item_t& root) {
  struct cbor_pair kv = {
      .key = cbor_move(cbor_build_stringn(key.data(), key.size())),
      .value = cbor_move(cbor_build_stringn(value.data(), value.size()))};
  if (!cbor_map_add(&root, kv)) {
    return absl::InternalError(
        absl::StrCat("Failed to serialize ", key, " to CBOR"));
  }

  return absl::OkStatus();
}

absl::Status CborSerializeByteString(absl::string_view key,
                                     absl::string_view value,
                                     cbor_item_t& root) {
  struct cbor_pair kv = {
      .key = cbor_move(cbor_build_stringn(key.data(), key.size())),
      .value = cbor_move(cbor_build_bytestring(
          reinterpret_cast<const unsigned char*>(value.data()), value.size()))};
  if (!cbor_map_add(&root, kv)) {
    return absl::InternalError(
        absl::StrCat("Failed to serialize ", key, " to CBOR"));
  }

  return absl::OkStatus();
}

absl::StatusOr<std::string> GetCborSerializedResult(
    cbor_item_t& cbor_data_root) {
  const size_t cbor_serialized_data_size =
      cbor_serialized_size(&cbor_data_root);
  if (!cbor_serialized_data_size) {
    return absl::InternalError("Failed to serialize to CBOR (too large!)");
  }
  std::string byte_string;
  byte_string.resize(cbor_serialized_data_size);
  if (cbor_serialize(&cbor_data_root,
                     reinterpret_cast<unsigned char*>(byte_string.data()),
                     cbor_serialized_data_size) == 0) {
    return absl::InternalError("Failed to serialize to CBOR");
  }
  return byte_string;
}
}  // namespace kv_server
