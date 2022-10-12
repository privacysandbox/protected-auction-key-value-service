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

#include <fstream>
#include <iostream>

#include "absl/cleanup/cleanup.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/match.h"
#include "aws/core/Aws.h"
#include "components/data/blob_storage_client.h"

ABSL_FLAG(std::string, bucket, "", "cloud storage bucket name");

using fledge::kv_server::BlobReader;
using fledge::kv_server::BlobStorageClient;

constexpr std::string_view kCloudPrefix = "cloud://";

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

class CloudBlobReader : public BlobReader {
 public:
  CloudBlobReader() = delete;
  explicit CloudBlobReader(std::unique_ptr<BlobReader> reader)
      : reader_(std::move(reader)) {}
  std::istream& Stream() override { return reader_->Stream(); }
  bool CanSeek() const override { return false; }

 private:
  std::unique_ptr<BlobReader> reader_;
};

bool ListObjects(std::string bucket) {
  std::unique_ptr<BlobStorageClient> client = BlobStorageClient::Create();
  const BlobStorageClient::DataLocation location = {std::move(bucket)};
  const absl::StatusOr<std::vector<std::string>> keys =
      client->ListBlobs(location, {});
  if (!keys.ok()) {
    std::cerr << keys.status() << std::endl;
    return false;
  }
  for (const auto& key : *keys) {
    std::cout << key << std::endl;
  }
  return true;
}

bool DeleteObjects(std::string bucket, absl::Span<char*> keys) {
  std::unique_ptr<BlobStorageClient> client = BlobStorageClient::Create();
  BlobStorageClient::DataLocation location = {std::move(bucket)};
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

absl::StatusOr<std::unique_ptr<BlobReader>> GetSourceStream(
    BlobStorageClient* client, std::string bucket, std::string source) {
  if (source == "-") {
    return std::make_unique<StdinBlobReader>();
  }
  if (absl::StartsWith(source, kCloudPrefix)) {
    std::string name = source.substr(kCloudPrefix.size());
    BlobStorageClient::DataLocation location = {std::move(bucket),
                                                std::move(name)};
    return std::make_unique<CloudBlobReader>(client->GetBlobReader(location));
  }
  auto fr = std::make_unique<FileBlobReader>(source);
  if (!fr->IsOpen()) {
    return absl::UnavailableError("Failed to open " + source);
  }
  return fr;
}

// Source and Destination files can be cloud files, local files, and stdin.
// Cloud files are prefixed with `cloud://`, stdin is `-`
bool CpObjects(std::string bucket, std::string source, std::string dest) {
  std::unique_ptr<BlobStorageClient> client = BlobStorageClient::Create();
  absl::StatusOr<std::unique_ptr<BlobReader>> reader =
      GetSourceStream(client.get(), bucket, std::move(source));
  if (!reader.ok()) {
    std::cerr << reader.status() << std::endl;
    return false;
  }

  if (absl::StartsWith(dest, kCloudPrefix)) {
    std::string name = dest.substr(kCloudPrefix.size());
    const absl::Status status =
        client->PutBlob(**reader, {std::move(bucket), std::move(name)});
    if (!status.ok()) {
      std::cerr << status << std::endl;
      return false;
    }
    return true;
  }
  if (dest == "-") {
    std::cout << (*reader)->Stream().rdbuf();
    return true;
  }
  std::ofstream ofile(dest);
  if (!ofile.is_open()) {
    std::cerr << "Failed to open " << dest << std::endl;
    return false;
  }
  ofile << (*reader)->Stream().rdbuf();
  ofile.close();
  return true;
}

bool CatObjects(std::string bucket, absl::Span<char*> keys) {
  std::unique_ptr<BlobStorageClient> client = BlobStorageClient::Create();
  BlobStorageClient::DataLocation location = {std::move(bucket)};
  for (const auto& key : keys) {
    location.key = key;
    auto reader = client->GetBlobReader(location);
    std::cout << reader->Stream().rdbuf();
  }
  return true;
}

int main(int argc, char** argv) {
  // TODO: use cc/cpio/cloud_providers to initialize cloud.
  Aws::SDKOptions options;
  Aws::InitAPI(options);
  absl::Cleanup shutdown = [&options] { Aws::ShutdownAPI(options); };

  absl::SetProgramUsageMessage("[cat|cp|ls|rm]");
  std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  if (commands.size() < 2) {
    std::cerr << "Must specify command: " << absl::ProgramUsageMessage()
              << std::endl;
    return -1;
  }
  std::string bucket = absl::GetFlag(FLAGS_bucket);
  if (!bucket.size()) {
    std::cerr << "Must specify bucket" << std::endl;
    return -1;
  }
  absl::string_view operation = commands[1];
  if (operation == "ls") {
    if (commands.size() != 2) {
      std::cerr << "ls does not take any extra arguments." << std::endl;
      return -1;
    }
    return ListObjects(std::move(bucket)) ? 0 : -1;
  }
  if (operation == "rm") {
    if (commands.size() < 3) {
      std::cerr << "Did not take get key name(s)." << std::endl;
      return -1;
    }
    return DeleteObjects(std::move(bucket), absl::MakeSpan(commands).subspan(2))
               ? 0
               : -1;
  }
  if (operation == "cat") {
    if (commands.size() < 3) {
      std::cerr << "Did not take get key name(s)." << std::endl;
      return -1;
    }
    return CatObjects(std::move(bucket), absl::MakeSpan(commands).subspan(2))
               ? 0
               : -1;
  }
  if (operation == "cp") {
    if (commands.size() != 4) {
      std::cerr << "Expected args [source] [destination]" << std::endl;
      return -1;
    }
    return CpObjects(std::move(bucket), commands[2], commands[3]) ? 0 : -1;
  }
  return 0;
}
