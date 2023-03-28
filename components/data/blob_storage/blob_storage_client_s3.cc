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

#include <iostream>
#include <thread>

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
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/blob_storage/seeking_input_streambuf.h"
#include "components/errors/error_util_aws.h"
#include "glog/logging.h"

namespace kv_server {
namespace {

// TODO(b/242313617): Make this a flag or parameter.
constexpr int64_t kMaxRangeBytes = 1024 * 1024 * 8;

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
    request.SetKey(location_.key);
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
    request.SetKey(location_.key);
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
  S3BlobReader(Aws::S3::S3Client& client,
               BlobStorageClient::DataLocation location,
               const BlobStorageClient::ClientOptions& client_options)
      : BlobReader(),
        streambuf_(client, location,
                   GetOptions(client_options.max_range_bytes,
                              [this, location](absl::Status status) {
                                LOG(ERROR) << "Blob " << location.key
                                           << " failed stream with: " << status;
                                is_.setstate(std::ios_base::badbit);
                              })),
        is_(&streambuf_) {}

  std::istream& Stream() { return is_; }
  bool CanSeek() const { return true; }

 private:
  static SeekingInputStreambuf::Options GetOptions(
      int64_t buffer_size, std::function<void(absl::Status)> error_callback) {
    SeekingInputStreambuf::Options options;
    options.buffer_size = buffer_size;
    options.error_callback = std::move(error_callback);
    return options;
  }

  S3BlobInputStreamBuf streambuf_;
  std::istream is_;
};

class S3BlobStorageClient : public BlobStorageClient {
 public:
  std::unique_ptr<BlobReader> GetBlobReader(DataLocation location) override {
    return std::make_unique<S3BlobReader>(*client_, std::move(location),
                                          client_options_);
  }

  absl::Status PutBlob(BlobReader& reader, DataLocation location) override {
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
        location.bucket, location.key, "", {});
    handle->WaitUntilFinished();
    const bool success =
        handle->GetStatus() == Aws::Transfer::TransferStatus::COMPLETED;
    return success ? absl::OkStatus()
                   : AwsErrorToStatus(handle->GetLastError());
  }

  absl::Status DeleteBlob(DataLocation location) override {
    Aws::S3::Model::DeleteObjectRequest request;
    request.SetBucket(std::move(location.bucket));
    request.SetKey(std::move(location.key));
    const auto outcome = client_->DeleteObject(request);
    return outcome.IsSuccess() ? absl::OkStatus()
                               : AwsErrorToStatus(outcome.GetError());
  }

  absl::StatusOr<std::vector<std::string>> ListBlobs(
      DataLocation location, ListOptions options) override {
    Aws::S3::Model::ListObjectsV2Request request;
    request.SetBucket(std::move(location.bucket));
    if (!options.prefix.empty()) {
      request.SetPrefix(std::move(options.prefix));
    }
    if (!options.start_after.empty()) {
      request.SetStartAfter(std::move(options.start_after));
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
        keys.push_back(object.GetKey());
      }
      done = !outcome.GetResult().GetIsTruncated();
      if (!done) {
        request.SetContinuationToken(
            outcome.GetResult().GetNextContinuationToken());
      }
    }
    return keys;
  }

  explicit S3BlobStorageClient(BlobStorageClient::ClientOptions client_options)
      : client_options_(std::move(client_options)) {
    Aws::Client::ClientConfiguration config;
    config.maxConnections = client_options_.max_connections;
    client_ = std::make_shared<Aws::S3::S3Client>(config);
    executor_ = std::make_unique<Aws::Utils::Threading::PooledThreadExecutor>(
        std::thread::hardware_concurrency());
    Aws::Transfer::TransferManagerConfiguration transfer_config(
        executor_.get());
    transfer_config.s3Client = client_;
    transfer_manager_ = Aws::Transfer::TransferManager::Create(transfer_config);
  }

 private:
  // TODO: Consider switch to CRT client.
  // AWS API requires shared_ptr
  ClientOptions client_options_;
  std::unique_ptr<Aws::Utils::Threading::PooledThreadExecutor> executor_;
  std::shared_ptr<Aws::S3::S3Client> client_;
  std::shared_ptr<Aws::Transfer::TransferManager> transfer_manager_;
};
}  // namespace

std::unique_ptr<BlobStorageClient> BlobStorageClient::Create(
    BlobStorageClient::ClientOptions client_options) {
  return std::make_unique<S3BlobStorageClient>(client_options);
}
}  // namespace kv_server
