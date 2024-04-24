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

#include "components/data/blob_storage/blob_storage_client_local.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/blob_storage/seeking_input_streambuf.h"

namespace kv_server {
namespace {

class FileBlobReader : public BlobReader {
 public:
  explicit FileBlobReader(const std::string& filename)
      : file_stream_(filename) {}
  ~FileBlobReader() = default;
  std::istream& Stream() override { return file_stream_; }
  bool CanSeek() const override { return true; }

 private:
  std::ifstream file_stream_;
};
}  // namespace

std::unique_ptr<BlobReader> FileBlobStorageClient::GetBlobReader(
    DataLocation location) {
  std::unique_ptr<BlobReader> reader =
      std::make_unique<FileBlobReader>(GetFullPath(location));

  if (!reader->Stream()) {
    PS_LOG(ERROR, log_context_) << absl::ErrnoToStatus(
        errno,
        absl::StrCat("Unable to open file: ", GetFullPath(location).string()));
    return nullptr;
  }
  return reader;
}
absl::Status FileBlobStorageClient::PutBlob(BlobReader& blob_reader,
                                            DataLocation location) {
  // Explicitly open the stream with 'out' so that it will overwrite any
  // existing file contents.
  std::ofstream blob_ostream(GetFullPath(location), std::ios_base::out);
  if (!blob_ostream) {
    return absl::ErrnoToStatus(
        errno,
        absl::StrCat("Unable to open file: ", GetFullPath(location).string()));
  }
  blob_ostream << blob_reader.Stream().rdbuf();
  blob_ostream.close();
  if (!blob_ostream) {
    return absl::ErrnoToStatus(errno,
                               absl::StrCat("Unable to write to file: ",
                                            GetFullPath(location).string()));
  }
  return absl::OkStatus();
}
absl::Status FileBlobStorageClient::DeleteBlob(DataLocation location) {
  auto fullpath = GetFullPath(location);
  std::error_code error_code;
  if (std::filesystem::remove(fullpath, error_code)) {
    return absl::OkStatus();
  }
  return absl::InternalError(
      absl::StrCat("Failed to delete blob: ", error_code.message()));
}
absl::StatusOr<std::vector<std::string>> FileBlobStorageClient::ListBlobs(
    DataLocation location, ListOptions options) {
  auto directory_name =
      location.prefix.empty()
          ? location.bucket
          : absl::StrCat(location.bucket, "/", location.prefix);
  {
    std::error_code error_code;
    std::filesystem::directory_entry directory{directory_name, error_code};
    if (error_code) {
      return absl::InternalError(absl::StrCat("Error getting directory entry: ",
                                              error_code.message()));
    }
  }
  std::error_code error_code;
  std::vector<std::string> blob_names;
  for (const auto& dir_entry :
       std::filesystem::directory_iterator(directory_name, error_code)) {
    if (dir_entry.is_directory()) {
      continue;
    }
    auto blob_name = dir_entry.path().filename();
    if (!absl::StartsWith(blob_name.string(), options.prefix) ||
        blob_name <= options.start_after) {
      continue;
    }
    blob_names.push_back(std::move(blob_name));
  }

  if (error_code) {
    return absl::InternalError(
        absl::StrCat("Error deleting blob: ", error_code.message()));
  }
  std::sort(blob_names.begin(), blob_names.end());
  return blob_names;
}

std::filesystem::path FileBlobStorageClient::GetFullPath(
    const DataLocation& location) {
  return location.prefix.empty()
             ? std::filesystem::path(location.bucket) / location.key
             : std::filesystem::path(location.bucket) / location.prefix /
                   location.key;
}

namespace {
class LocalBlobStorageClientFactory : public BlobStorageClientFactory {
 public:
  ~LocalBlobStorageClientFactory() = default;
  std::unique_ptr<BlobStorageClient> CreateBlobStorageClient(
      BlobStorageClient::ClientOptions /*client_options*/,
      privacy_sandbox::server_common::log::PSLogContext& log_context) override {
    return std::make_unique<FileBlobStorageClient>(log_context);
  }
};
}  // namespace

std::unique_ptr<BlobStorageClientFactory> BlobStorageClientFactory::Create() {
  return std::make_unique<LocalBlobStorageClientFactory>();
}

}  // namespace kv_server
