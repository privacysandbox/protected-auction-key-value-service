// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "components/internal_server/string_padder.h"

#include <string>

#include "absl/log/log.h"
#include "quiche/common/quiche_data_reader.h"
#include "quiche/common/quiche_data_writer.h"

namespace kv_server {

std::string Pad(std::string_view string_to_pad, int32_t extra_padding) {
  int output_size = sizeof(u_int32_t) + string_to_pad.size() + extra_padding;
  std::string output(output_size, '0');

  quiche::QuicheDataWriter data_writer(output.size(), output.data());
  data_writer.WriteUInt32(string_to_pad.size());
  data_writer.WriteStringPiece(string_to_pad);
  return output;
}

absl::StatusOr<std::string> Unpad(std::string_view padded_string) {
  auto data_reader = quiche::QuicheDataReader(padded_string);
  uint32_t string_size = 0;
  if (!data_reader.ReadUInt32(&string_size)) {
    return absl::InvalidArgumentError("Failed to read string size");
  }
  VLOG(9) << "string size: " << string_size;
  std::string_view output;
  if (!data_reader.ReadStringPiece(&output, string_size)) {
    return absl::InvalidArgumentError("Failed to read a string");
  }
  VLOG(9) << "string: " << output;
  return std::string(output);
}

}  // namespace kv_server
