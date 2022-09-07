/*
 * Copyright 2022 Google LLC
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

#ifndef COMPONENTS_DATA_BLOB_STORAGE_CLIENT_H_
#define COMPONENTS_DATA_BLOB_STORAGE_CLIENT_H_

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace fledge::kv_server {

// Contains a stream of content data read from a cloud object.
class BlobReader {
 public:
  virtual ~BlobReader() = default;
  virtual std::istream& Stream() = 0;
  // True if the istream returned by `Stream` supports `seek`.
  virtual bool CanSeek() const = 0;
};

// Abstraction to interact with cloud file storage.
class BlobStorageClient {
 public:
  struct DataLocation {
    std::string bucket;
    std::string key;

    bool operator==(const DataLocation& other) const {
      return key == other.key && bucket == other.bucket;
    }
  };
  struct ListOptions {
    std::string prefix;
    std::string start_after;  // noninclusive
  };

  static std::unique_ptr<BlobStorageClient> Create();
  virtual ~BlobStorageClient() = default;

  // Download blob content.
  virtual absl::StatusOr<std::unique_ptr<BlobReader>> GetBlob(
      DataLocation location) = 0;

  // Upload blob content.
  // Underlying client libraries require iostream.
  virtual absl::Status PutBlob(BlobReader&, DataLocation location) = 0;

  // Delete single file.
  virtual absl::Status DeleteBlob(DataLocation location) = 0;

  // Get `key` for every object in the provided `bucket`
  // Keys are lexicographically ordered.
  virtual absl::StatusOr<std::vector<std::string>> ListBlobs(
      DataLocation location, ListOptions options) = 0;
};

inline std::ostream& operator<<(
    std::ostream& os, const BlobStorageClient::DataLocation& location) {
  os << location.bucket << "/" << location.key;
  return os;
}

}  // namespace fledge::kv_server

#endif  // COMPONENTS_DATA_BLOB_STORAGE_CLIENT_H_
