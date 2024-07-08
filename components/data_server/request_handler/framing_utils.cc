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
#include "components/data_server/request_handler/framing_utils.h"

#include <algorithm>

#include "absl/numeric/bits.h"

namespace kv_server {

// 1 byte for version + compression details.
constexpr int kVersionCompressionSize = 1;

// 4-bytes specifying the size of the actual payload.
constexpr int kPayloadLength = 4;

// Minimum size of the returned response in bytes.
// TODO: b/348613920 - Move framing utils to the common repo, and as part of
// that figure out if this needs to be inline with B&A.
inline constexpr size_t kMinResultBytes = 0;

// Gets size of the complete payload including the preamble expected by
// android, which is: 1 byte (containing version, compression details), 4 bytes
// indicating the length of the actual encoded response and any other padding
// required to make the complete payload a power of 2.
size_t GetEncodedDataSize(size_t encapsulated_payload_size) {
  size_t total_payload_size =
      kVersionCompressionSize + kPayloadLength + encapsulated_payload_size;
  // Ensure that the payload size is a power of 2.
  return std::max(absl::bit_ceil(total_payload_size), kMinResultBytes);
}

}  // namespace kv_server
