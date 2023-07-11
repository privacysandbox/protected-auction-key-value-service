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
#include "src/cpp/telemetry/metrics_recorder.h"

namespace Aws {
namespace S3 {
class S3Client;
}  // namespace S3
}  // namespace Aws

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
    std::string key;

    bool operator==(const DataLocation& other) const {
      return key == other.key && bucket == other.bucket;
    }
  };
  struct ListOptions {
    std::string prefix;
    std::string start_after;  // noninclusive
  };

  // Options for the underyling storage client.
  struct ClientOptions {
    ClientOptions() {}
    int64_t max_connections = std::thread::hardware_concurrency();
    int64_t max_range_bytes = 8 * 1024 * 1024;  // 8MB

    // BlobStorageClient takes ownership of this if it's set:
    ::Aws::S3::S3Client* s3_client_for_unit_testing_ = nullptr;
  };

  // TODO(b/237669491): Replace these factory methods with one based off the
  // flag values that are set.
  static std::unique_ptr<BlobStorageClient> Create(
      privacy_sandbox::server_common::MetricsRecorder& metrics_recorder,
      ClientOptions client_options = ClientOptions());
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
  os << location.bucket << "/" << location.key;
  return os;
}

}  // namespace kv_server

#endif  // COMPONENTS_DATA_BLOB_STORAGE_BLOB_STORAGE_CLIENT_H_
