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
#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "components/data_server/request_handler/compression/compression.h"

namespace kv_server {

// Builds compression groups that are not using any compression algorithm.
class UncompressedConcatenator : public CompressionGroupConcatenator {
 public:
  absl::StatusOr<std::string> Build() const override;
};

// Reads compression groups built with UncompressedConcatenator.
class UncompressedBlobReader : public CompressedBlobReader {
 public:
  explicit UncompressedBlobReader(std::string_view compressed)
      : CompressedBlobReader(compressed) {}

  absl::StatusOr<std::string> ExtractOneCompressionGroup() override;
};

}  // namespace kv_server
