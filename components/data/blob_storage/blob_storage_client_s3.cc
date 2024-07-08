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

#include "components/data/blob_storage/blob_storage_client_s3.h"

#include <cstdint>
#include <iostream>
#include <thread>
#include <utility>

#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "aws/core/Aws.h"
#include "aws/core/utils/threading/Executor.h"
#include "aws/s3/S3Client.h"
#include "aws/s3/model/Bucket.h"
#include "aws/s3/model/DeleteObjectRequest.h"
#include "aws/s3/model/GetObjectRequest.h"
#include "aws/s3/model/HeadObjectRequest.h"
#include "aws/s3/model/ListObjectsV2Request.h"
#include "aws/s3/model/Object.h"
#include "aws/s3/model/PutObjectRequest.h"
#include "aws/transfer/TransferHandle.h"
#include "aws/transfer/TransferManager.h"
#include "components/data/blob_storage/blob_prefix_allowlist.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/blob_storage/seeking_input_streambuf.h"
#include "components/errors/error_util_aws.h"

namespace kv_server {
namespace {

std::string AppendPrefix(const std::string& value, const std::string& prefix) {
  return prefix.empty() ? value : absl::StrCat(prefix, "/", value);
}

// Sequentially load byte range data with a fixed amount of memory usage.
class S3BlobInputStreamBuf : public SeekingInputStreambuf {
 public:
  S3BlobInputStreamBuf(Aws::S3::S3Client& client,
                       BlobStorageClient::DataLocation location,
                       SeekingInputStreambuf::Options options)
      : SeekingInputStreambuf(std::move(options)),
        client_(client),
        location_(std::move(location)) {}

  S3BlobInputStreamBuf(const S3BlobInputStreamBuf&) = delete;
  S3BlobInputStreamBuf& operator=(const S3BlobInputStreamBuf&) = delete;

 protected:
  absl::StatusOr<int64_t> SizeImpl() override {
    Aws::S3::Model::HeadObjectRequest request;
    request.SetBucket(location_.bucket);
    request.SetKey(AppendPrefix(location_.key, location_.prefix));
    auto outcome = client_.HeadObject(request);
    if (!outcome.IsSuccess()) {
      return AwsErrorToStatus(outcome.GetError());
    }
    return outcome.GetResultWithOwnership().GetContentLength();
  }

  absl::StatusOr<int64_t> ReadChunk(int64_t offset, int64_t chunk_size,
                                    char* dest_buffer) override {
    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(location_.bucket);
    request.SetKey(AppendPrefix(location_.key, location_.prefix));
    request.SetRange(GetRange(offset, chunk_size));
    auto outcome = client_.GetObject(request);
    if (!outcome.IsSuccess()) {
      return AwsErrorToStatus(outcome.GetError());
    }
    auto& stream = outcome.GetResultWithOwnership().GetBody();
    stream.seekg(0, stream.end);
    const uint64_t bytes_read = stream.tellg();
    stream.seekg(0, stream.beg);
    stream.read(dest_buffer, bytes_read);
    return bytes_read;
  }

 private:
  std::string GetRange(uint64_t offset, uint64_t length) {
    // Here the range end needs to be `offset + length - 1` because byte ranges
    // are inclusive of both range boundaries, so `bytes=0-9` downloads 10
    // bytes.
    return absl::StrCat("bytes=", std::to_string(offset), "-",
                        std::to_string(offset + length - 1));
  }

  Aws::S3::S3Client& client_;
  const BlobStorageClient::DataLocation location_;
};

class S3BlobReader : public BlobReader {
 public:
  S3BlobReader(
      Aws::S3::S3Client& client, BlobStorageClient::DataLocation location,
      int64_t max_range_bytes,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext))
      : BlobReader(),
        log_context_(log_context),
        streambuf_(client, location,
                   GetOptions(
                       max_range_bytes,
                       [this, location](absl::Status status) {
                         PS_LOG(ERROR, log_context_)
                             << "Blob " << location.key
                             << " failed stream with: " << status;
                         is_.setstate(std::ios_base::badbit);
                       },
                       log_context)),
        is_(&streambuf_) {}

  std::istream& Stream() { return is_; }
  bool CanSeek() const { return true; }

 private:
  static SeekingInputStreambuf::Options GetOptions(
      int64_t buffer_size, std::function<void(absl::Status)> error_callback,
      privacy_sandbox::server_common::log::PSLogContext& log_context) {
    SeekingInputStreambuf::Options options;
    options.buffer_size = buffer_size;
    options.error_callback = std::move(error_callback);
    options.log_context = log_context;
    return options;
  }
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
  S3BlobInputStreamBuf streambuf_;
  std::istream is_;
};
}  // namespace

S3BlobStorageClient::S3BlobStorageClient(
    std::shared_ptr<Aws::S3::S3Client> client, int64_t max_range_bytes,
    privacy_sandbox::server_common::log::PSLogContext& log_context)
    : client_(client),
      max_range_bytes_(max_range_bytes),
      log_context_(log_context) {
  executor_ = std::make_unique<Aws::Utils::Threading::PooledThreadExecutor>(
      std::thread::hardware_concurrency());
  Aws::Transfer::TransferManagerConfiguration transfer_config(executor_.get());
  transfer_config.s3Client = client_;
  transfer_manager_ = Aws::Transfer::TransferManager::Create(transfer_config);
}

std::unique_ptr<BlobReader> S3BlobStorageClient::GetBlobReader(
    DataLocation location) {
  return std::make_unique<S3BlobReader>(*client_, std::move(location),
                                        max_range_bytes_, log_context_);
}

absl::Status S3BlobStorageClient::PutBlob(BlobReader& reader,
                                          DataLocation location) {
  std::unique_ptr<std::iostream> iostream;
  std::stringstream ss;
  if (reader.CanSeek()) {
    iostream = std::make_unique<std::iostream>(reader.Stream().rdbuf());
  } else {
    // TODO: Do a manual multipart upload
    ss << reader.Stream().rdbuf();
    iostream = std::make_unique<std::iostream>(ss.rdbuf());
  }
  // S3 requires a shared_pointer, other platforms do not.
  // Wrap the raw pointer as a shared_ptr and don't deallocate.
  // The owner of the stream is the caller.
  auto handle = transfer_manager_->UploadFile(
      std::shared_ptr<std::iostream>(iostream.get(), [](std::iostream*) {}),
      location.bucket, AppendPrefix(location.key, location.prefix), "", {});
  handle->WaitUntilFinished();
  const bool success =
      handle->GetStatus() == Aws::Transfer::TransferStatus::COMPLETED;
  return success ? absl::OkStatus() : AwsErrorToStatus(handle->GetLastError());
}

absl::Status S3BlobStorageClient::DeleteBlob(DataLocation location) {
  Aws::S3::Model::DeleteObjectRequest request;
  request.SetBucket(std::move(location.bucket));
  request.SetKey(AppendPrefix(location.key, location.prefix));
  const auto outcome = client_->DeleteObject(request);
  return outcome.IsSuccess() ? absl::OkStatus()
                             : AwsErrorToStatus(outcome.GetError());
}

absl::StatusOr<std::vector<std::string>> S3BlobStorageClient::ListBlobs(
    DataLocation location, ListOptions options) {
  Aws::S3::Model::ListObjectsV2Request request;
  request.SetBucket(std::move(location.bucket));
  if (!options.prefix.empty()) {
    request.SetPrefix(
        AppendPrefix(/*value=*/options.prefix, /*prefix=*/location.prefix));
  }
  if (!options.start_after.empty()) {
    request.SetStartAfter(AppendPrefix(/*value=*/options.start_after,
                                       /*prefix=*/location.prefix));
  }
  bool done = false;
  std::vector<std::string> keys;
  while (!done) {
    const auto outcome = client_->ListObjectsV2(request);
    if (!outcome.IsSuccess()) {
      return AwsErrorToStatus(outcome.GetError());
    }
    const Aws::Vector<Aws::S3::Model::Object> objects =
        outcome.GetResult().GetContents();
    for (const Aws::S3::Model::Object& object : objects) {
      if (auto blob = ParseBlobName(object.GetKey());
          blob.prefix == location.prefix) {
        keys.emplace_back(std::move(blob.key));
      }
    }
    done = !outcome.GetResult().GetIsTruncated();
    if (!done) {
      request.SetContinuationToken(
          outcome.GetResult().GetNextContinuationToken());
    }
  }
  return keys;
}

namespace {
class S3BlobStorageClientFactory : public BlobStorageClientFactory {
 public:
  ~S3BlobStorageClientFactory() = default;
  std::unique_ptr<BlobStorageClient> CreateBlobStorageClient(
      BlobStorageClient::ClientOptions client_options,
      privacy_sandbox::server_common::log::PSLogContext& log_context) override {
    Aws::Client::ClientConfiguration config;
    config.maxConnections = client_options.max_connections;
    std::shared_ptr<Aws::S3::S3Client> client =
        std::make_shared<Aws::S3::S3Client>(config);

    return std::make_unique<S3BlobStorageClient>(
        client, client_options.max_range_bytes, log_context);
  }
};
}  // namespace

std::unique_ptr<BlobStorageClientFactory> BlobStorageClientFactory::Create() {
  return std::make_unique<S3BlobStorageClientFactory>();
}

}  // namespace kv_server
