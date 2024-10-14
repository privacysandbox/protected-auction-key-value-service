// Copyright 2022 Google LLC
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
#include "components/data_server/request_handler/compression/uncompressed.h"

#include <string>

#include "absl/log/log.h"
#include "quiche/common/quiche_data_writer.h"

namespace kv_server {

absl::StatusOr<std::string> UncompressedConcatenator::Build() const {
  std::string output;
  int output_size = sizeof(u_int32_t) * Partitions().size();
  for (const auto& partition : Partitions()) {
    output_size += partition.size();
  }

  output.resize(output_size);
  quiche::QuicheDataWriter data_writer(output.size(), output.data());
  for (const auto& partition : Partitions()) {
    data_writer.WriteUInt32(partition.size());
    data_writer.WriteStringPiece(partition);
  }
  return output;
}

absl::StatusOr<std::string>
UncompressedBlobReader::ExtractOneCompressionGroup() {
  uint32_t compression_group_size = 0;
  if (!data_reader_.ReadUInt32(&compression_group_size)) {
    return absl::InvalidArgumentError("Failed to read compression group size");
  }
  VLOG(9) << "compression_group_size: " << compression_group_size;
  std::string_view output;
  if (!data_reader_.ReadStringPiece(&output, compression_group_size)) {
    return absl::InvalidArgumentError("Failed to read compression group");
  }
  VLOG(9) << "compression group: " << output;
  return std::string(output);
}

}  // namespace kv_server
