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

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "aws/core/utils/threading/Executor.h"
#include "aws/s3/S3Client.h"
#include "aws/transfer/TransferManager.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "src/logger/request_context_logger.h"

namespace kv_server {

class S3BlobStorageClient : public BlobStorageClient {
 public:
  explicit S3BlobStorageClient(
      std::shared_ptr<Aws::S3::S3Client> client, int64_t max_range_bytes,
      privacy_sandbox::server_common::log::PSLogContext& log_context);

  ~S3BlobStorageClient() = default;

  std::unique_ptr<BlobReader> GetBlobReader(DataLocation location) override;

  absl::Status PutBlob(BlobReader& reader, DataLocation location) override;

  absl::Status DeleteBlob(DataLocation location) override;

  absl::StatusOr<std::vector<std::string>> ListBlobs(
      DataLocation location, ListOptions options) override;

 private:
  // TODO: Consider switch to CRT client.
  // AWS API requires shared_ptr
  std::unique_ptr<Aws::Utils::Threading::PooledThreadExecutor> executor_;
  std::shared_ptr<Aws::S3::S3Client> client_;
  std::shared_ptr<Aws::Transfer::TransferManager> transfer_manager_;
  int64_t max_range_bytes_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};
}  // namespace kv_server
