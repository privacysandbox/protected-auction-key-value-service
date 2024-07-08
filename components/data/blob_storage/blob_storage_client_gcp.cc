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

#include "components/data/blob_storage/blob_storage_client_gcp.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "components/data/blob_storage/blob_prefix_allowlist.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/blob_storage/seeking_input_streambuf.h"
#include "components/errors/error_util_gcp.h"
#include "google/cloud/storage/client.h"

namespace kv_server {
namespace {

std::string AppendPrefix(const std::string& value, const std::string& prefix) {
  return prefix.empty() ? value : absl::StrCat(prefix, "/", value);
}

class GcpBlobInputStreamBuf : public SeekingInputStreambuf {
 public:
  GcpBlobInputStreamBuf(google::cloud::storage::Client& client,
                        BlobStorageClient::DataLocation location,
                        SeekingInputStreambuf::Options options)
      : SeekingInputStreambuf(std::move(options)),
        client_(client),
        location_(std::move(location)) {}

  GcpBlobInputStreamBuf(const GcpBlobInputStreamBuf&) = delete;
  GcpBlobInputStreamBuf& operator=(const GcpBlobInputStreamBuf&) = delete;

 protected:
  absl::StatusOr<int64_t> SizeImpl() override {
    auto object_metadata = client_.GetObjectMetadata(
        location_.bucket, AppendPrefix(location_.key, location_.prefix));
    if (!object_metadata) {
      return GoogleErrorStatusToAbslStatus(object_metadata.status());
    }
    return object_metadata->size();
  }

  absl::StatusOr<int64_t> ReadChunk(int64_t offset, int64_t chunk_size,
                                    char* dest_buffer) override {
    auto stream = client_.ReadObject(
        location_.bucket, AppendPrefix(location_.key, location_.prefix),
        google::cloud::storage::ReadRange(offset, offset + chunk_size));
    if (!stream.status().ok()) {
      return GoogleErrorStatusToAbslStatus(stream.status());
    }
    std::string contents(std::istreambuf_iterator<char>{stream}, {});
    int64_t true_size = contents.size();
    std::copy(contents.begin(), contents.end(), dest_buffer);
    return true_size;
  }

 private:
  google::cloud::storage::Client& client_;
  const BlobStorageClient::DataLocation location_;
};

class GcpBlobReader : public BlobReader {
 public:
  GcpBlobReader(
      google::cloud::storage::Client& client,
      BlobStorageClient::DataLocation location,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext))
      : BlobReader(),
        log_context_(log_context),
        streambuf_(client, location,
                   GetOptions(
                       [this, location](absl::Status status) {
                         PS_LOG(ERROR, log_context_)
                             << "Blob "
                             << AppendPrefix(location.key, location.prefix)
                             << " failed stream with: " << status;
                         is_.setstate(std::ios_base::badbit);
                       },
                       log_context)),
        is_(&streambuf_) {}

  std::istream& Stream() { return is_; }
  bool CanSeek() const { return true; }

 private:
  static SeekingInputStreambuf::Options GetOptions(
      std::function<void(absl::Status)> error_callback,
      privacy_sandbox::server_common::log::PSLogContext& log_context) {
    SeekingInputStreambuf::Options options;
    options.error_callback = std::move(error_callback);
    options.log_context = log_context;
    return options;
  }
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
  GcpBlobInputStreamBuf streambuf_;
  std::istream is_;
};
}  // namespace

GcpBlobStorageClient::GcpBlobStorageClient(
    std::unique_ptr<google::cloud::storage::Client> client,
    privacy_sandbox::server_common::log::PSLogContext& log_context)
    : client_(std::move(client)), log_context_(log_context) {}

std::unique_ptr<BlobReader> GcpBlobStorageClient::GetBlobReader(
    DataLocation location) {
  return std::make_unique<GcpBlobReader>(*client_, std::move(location),
                                         log_context_);
}

absl::Status GcpBlobStorageClient::PutBlob(BlobReader& blob_reader,
                                           DataLocation location) {
  auto blob_ostream = client_->WriteObject(
      location.bucket, AppendPrefix(location.key, location.prefix));
  if (!blob_ostream) {
    return GoogleErrorStatusToAbslStatus(blob_ostream.last_status());
  }
  blob_ostream << blob_reader.Stream().rdbuf();
  blob_ostream.Close();
  return blob_ostream
             ? absl::OkStatus()
             : GoogleErrorStatusToAbslStatus(blob_ostream.last_status());
}

absl::Status GcpBlobStorageClient::DeleteBlob(DataLocation location) {
  google::cloud::Status status = client_->DeleteObject(
      location.bucket, AppendPrefix(location.key, location.prefix));
  return status.ok() ? absl::OkStatus() : GoogleErrorStatusToAbslStatus(status);
}

absl::StatusOr<std::vector<std::string>> GcpBlobStorageClient::ListBlobs(
    DataLocation location, ListOptions options) {
  auto list_object_reader =
      client_->ListObjects(location.bucket,
                           google::cloud::storage::Prefix(
                               AppendPrefix(options.prefix, location.prefix)),
                           google::cloud::storage::StartOffset(AppendPrefix(
                               options.start_after, location.prefix)));
  std::vector<std::string> keys;
  if (list_object_reader.begin() == list_object_reader.end()) {
    return keys;
  }
  for (auto&& object_metadata : list_object_reader) {
    if (!object_metadata) {
      PS_LOG(ERROR, log_context_)
          << "Blob error when listing blobs:"
          << std::move(object_metadata).status().message();
      continue;
    }
    // Manually exclude the starting name as the StartOffset option is
    // inclusive and also drop blobs with different prefix.
    auto blob = ParseBlobName(object_metadata->name());
    if (blob.key == options.start_after || blob.prefix != location.prefix) {
      continue;
    }
    keys.push_back(std::move(blob.key));
  }
  std::sort(keys.begin(), keys.end());
  return keys;
}

namespace {
class GcpBlobStorageClientFactory : public BlobStorageClientFactory {
 public:
  ~GcpBlobStorageClientFactory() = default;
  std::unique_ptr<BlobStorageClient> CreateBlobStorageClient(
      BlobStorageClient::ClientOptions /*client_options*/,
      privacy_sandbox::server_common::log::PSLogContext& log_context) override {
    return std::make_unique<GcpBlobStorageClient>(
        std::make_unique<google::cloud::storage::Client>(), log_context);
  }
};
}  // namespace

std::unique_ptr<BlobStorageClientFactory> BlobStorageClientFactory::Create() {
  return std::make_unique<GcpBlobStorageClientFactory>();
}
}  // namespace kv_server
