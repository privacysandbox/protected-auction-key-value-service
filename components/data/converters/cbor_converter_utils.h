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

#ifndef COMPONENTS_DATA_CONVERTER_UTILS_H
#define COMPONENTS_DATA_CONVERTER_UTILS_H

#include <string>

#include "absl/status/statusor.h"

#include "cbor.h"

namespace kv_server {
cbor_item_t* cbor_build_uint(uint32_t input);

absl::Status CborSerializeUInt(absl::string_view key, uint32_t value,
                               cbor_item_t& root);

absl::Status CborSerializeString(absl::string_view key, absl::string_view value,
                                 cbor_item_t& root);

absl::Status CborSerializeByteString(absl::string_view key,
                                     absl::string_view value,
                                     cbor_item_t& root);

absl::StatusOr<std::string> GetCborSerializedResult(
    cbor_item_t& cbor_data_root);

}  // namespace kv_server
#endif  // COMPONENTS_DATA_CONVERTER_UTILS_H
