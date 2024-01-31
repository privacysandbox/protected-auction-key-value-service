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

#include "components/tools/blob_storage_commands.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "src/cpp/telemetry/telemetry_provider.h"

namespace kv_server {
namespace blob_storage_commands {
namespace {

using privacy_sandbox::server_common::TelemetryProvider;

class FileBlobReader : public BlobReader {
 public:
  explicit FileBlobReader(const std::string& filename)
      : stream_(filename, std::ifstream::in) {}
  ~FileBlobReader() { stream_.close(); }
  std::istream& Stream() override { return stream_; }
  bool CanSeek() const override { return true; }
  bool IsOpen() const { return stream_.is_open(); }

 private:
  std::ifstream stream_;
};

class StdinBlobReader : public BlobReader {
 public:
  std::istream& Stream() override { return std::cin; }
  bool CanSeek() const override { return false; };
};

}  // namespace

using kv_server::BlobStorageClient;

bool CatObjects(std::string bucket_or_directory, std::string prefix,
                absl::Span<char*> keys) {
  std::unique_ptr<BlobStorageClientFactory> blob_storage_client_factory =
      BlobStorageClientFactory::Create();
  std::unique_ptr<BlobStorageClient> client =
      blob_storage_client_factory->CreateBlobStorageClient();
  BlobStorageClient::DataLocation location = {
      .bucket = std::move(bucket_or_directory),
      .prefix = std::move(prefix),
  };
  for (const auto& key : keys) {
    location.key = key;
    auto reader = client->GetBlobReader(location);
    std::cout << reader->Stream().rdbuf();
  }
  return true;
}

bool DeleteObjects(std::string bucket_or_directory, std::string prefix,
                   absl::Span<char*> keys) {
  std::unique_ptr<BlobStorageClientFactory> blob_storage_client_factory =
      BlobStorageClientFactory::Create();
  std::unique_ptr<BlobStorageClient> client =
      blob_storage_client_factory->CreateBlobStorageClient();
  BlobStorageClient::DataLocation location = {
      .bucket = std::move(bucket_or_directory),
      .prefix = std::move(prefix),
  };
  for (const auto& key : keys) {
    location.key = key;
    const absl::Status status = client->DeleteBlob(location);
    if (!status.ok()) {
      std::cerr << status << std::endl;
      return false;
    }
  }
  return true;
}

bool ListObjects(std::string bucket_or_directory, std::string prefix) {
  std::unique_ptr<BlobStorageClientFactory> blob_storage_client_factory =
      BlobStorageClientFactory::Create();
  std::unique_ptr<BlobStorageClient> client =
      blob_storage_client_factory->CreateBlobStorageClient();
  const BlobStorageClient::DataLocation location = {
      .bucket = std::move(bucket_or_directory),
      .prefix = std::move(prefix),
  };
  const absl::StatusOr<std::vector<std::string>> keys =
      client->ListBlobs(location, {});
  if (!keys.ok()) {
    std::cerr << "Failed to list objects: " << keys.status() << std::endl;
    return false;
  }
  for (const auto& key : *keys) {
    std::cout << key << std::endl;
  }
  return true;
}

absl::StatusOr<std::unique_ptr<BlobReader>> GetStdinOrFileSourceStream(
    std::string directory, std::string source) {
  if (source == "-") {
    return std::make_unique<StdinBlobReader>();
  }

  const std::filesystem::path filepath =
      std::filesystem::path(directory) / source;

  auto file_reader = std::make_unique<FileBlobReader>(filepath.string());
  if (!file_reader->IsOpen()) {
    return absl::UnavailableError("Failed to open " + filepath.string());
  }
  return file_reader;
}

bool CopyFromReaderToStdoutOrFileDestination(std::unique_ptr<BlobReader> reader,
                                             std::string directory,
                                             std::string dest) {
  if (dest == "-") {
    std::cout << reader->Stream().rdbuf();
    return true;
  }

  const std::filesystem::path filepath =
      std::filesystem::path(directory) / dest;

  std::ofstream ofile(filepath);
  if (!ofile.is_open()) {
    std::cerr << "Failed to open " << filepath << std::endl;
    return false;
  }
  ofile << reader->Stream().rdbuf();
  ofile.close();
  return true;
}

}  // namespace blob_storage_commands
}  // namespace kv_server
