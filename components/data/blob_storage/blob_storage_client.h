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

#ifndef COMPONENTS_DATA_BLOB_STORAGE_BLOB_STORAGE_CLIENT_H_
#define COMPONENTS_DATA_BLOB_STORAGE_BLOB_STORAGE_CLIENT_H_

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "src/logger/request_context_logger.h"

namespace kv_server {

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
    std::string prefix;
    std::string key;

    bool operator==(const DataLocation& other) const {
      return prefix == other.prefix && key == other.key &&
             bucket == other.bucket;
    }
  };
  struct ListOptions {
    std::string prefix;
    std::string start_after;  // noninclusive
  };

  // Options for the underyling storage client.
  struct ClientOptions {
    ClientOptions() = default;
    int64_t max_connections = std::thread::hardware_concurrency();
    int64_t max_range_bytes = 8 * 1024 * 1024;  // 8MB
  };

  virtual ~BlobStorageClient() = default;

  // Get handle to blob data.
  virtual std::unique_ptr<BlobReader> GetBlobReader(DataLocation location) = 0;

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
  location.prefix.empty()
      ? os << location.bucket << "/" << location.key
      : os << location.bucket << "/" << location.prefix << "/" << location.key;
  return os;
}

class BlobStorageClientFactory {
 public:
  virtual ~BlobStorageClientFactory() = default;
  virtual std::unique_ptr<BlobStorageClient> CreateBlobStorageClient(
      BlobStorageClient::ClientOptions client_options =
          BlobStorageClient::ClientOptions(),
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext)) = 0;
  static std::unique_ptr<BlobStorageClientFactory> Create();
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_BLOB_STORAGE_BLOB_STORAGE_CLIENT_H_
