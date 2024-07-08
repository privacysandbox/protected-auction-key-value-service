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

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "google/cloud/storage/client.h"

namespace kv_server {

class GcpBlobStorageClient : public BlobStorageClient {
 public:
  explicit GcpBlobStorageClient(
      std::unique_ptr<google::cloud::storage::Client> client,
      privacy_sandbox::server_common::log::PSLogContext& log_context);

  ~GcpBlobStorageClient() = default;

  std::unique_ptr<BlobReader> GetBlobReader(DataLocation location) override;

  absl::Status PutBlob(BlobReader& blob_reader, DataLocation location) override;

  absl::Status DeleteBlob(DataLocation location) override;

  absl::StatusOr<std::vector<std::string>> ListBlobs(
      DataLocation location, ListOptions options) override;

 private:
  std::unique_ptr<google::cloud::storage::Client> client_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};
}  // namespace kv_server
