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
#include "components/data_server/request_handler/compression.h"

#include "components/data_server/request_handler/compression_brotli.h"
#include "components/data_server/request_handler/uncompressed.h"
#include "glog/logging.h"
#include "quiche/common/quiche_data_writer.h"

namespace kv_server {

void CompressionGroupConcatenator::AddCompressionGroup(
    std::string plaintext_compression_group) {
  VLOG(9) << "Adding compression group: " << plaintext_compression_group;
  partitions_.push_back(plaintext_compression_group);
}

std::unique_ptr<CompressionGroupConcatenator>
CompressionGroupConcatenator::Create(CompressionType type) {
  if (type == CompressionType::kUncompressed) {
    return std::make_unique<UncompressedConcatenator>();
  } else {
    return std::make_unique<BrotliCompressionGroupConcatenator>();
  }
}

std::unique_ptr<CompressedBlobReader> CompressedBlobReader::Create(
    CompressionGroupConcatenator::CompressionType type,
    std::string_view compressed) {
  if (type == CompressionGroupConcatenator::CompressionType::kUncompressed) {
    return std::make_unique<UncompressedBlobReader>(compressed);
  } else {
    return std::make_unique<BrotliCompressionBlobReader>(compressed);
  }
}

}  // namespace kv_server
