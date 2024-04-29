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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/match.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/tools/blob_storage_commands.h"
#include "components/tools/util/configure_telemetry_tools.h"
#include "components/util/platform_initializer.h"
#include "src/telemetry/telemetry_provider.h"

ABSL_FLAG(std::string, bucket, "", "cloud storage bucket name");
ABSL_FLAG(std::string, prefix, "", "object prefix name");

using kv_server::BlobReader;
using kv_server::BlobStorageClient;
using kv_server::BlobStorageClientFactory;
using privacy_sandbox::server_common::TelemetryProvider;

constexpr std::string_view kCloudPrefix = "cloud://";

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

absl::StatusOr<std::unique_ptr<BlobReader>> GetSourceStream(
    BlobStorageClient* client, std::string bucket, std::string source) {
  if (absl::StartsWith(source, kCloudPrefix)) {
    std::string name = source.substr(kCloudPrefix.size());
    BlobStorageClient::DataLocation location = {std::move(bucket),
                                                std::move(name)};
    return std::make_unique<CloudBlobReader>(client->GetBlobReader(location));
  }

  return kv_server::blob_storage_commands::GetStdinOrFileSourceStream(
      std::move(bucket), std::move(source));
}

// Source and Destination files can be cloud files, local files, and stdin.
// Cloud files are prefixed with `cloud://`, stdin is `-`
bool CpObjects(std::string bucket, std::string source, std::string dest) {
  std::unique_ptr<BlobStorageClientFactory> blob_storage_client_factory =
      BlobStorageClientFactory::Create();
  std::unique_ptr<BlobStorageClient> client =
      blob_storage_client_factory->CreateBlobStorageClient();
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

  return kv_server::blob_storage_commands::
      CopyFromReaderToStdoutOrFileDestination(std::move(*reader),
                                              std::move(bucket), dest);
}

int main(int argc, char** argv) {
  kv_server::PlatformInitializer initializer;
  kv_server::ConfigureTelemetryForTools();
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
  std::string prefix = absl::GetFlag(FLAGS_prefix);
  absl::string_view operation = commands[1];
  if (operation == "ls") {
    if (commands.size() != 2) {
      std::cerr << "ls does not take any extra arguments." << std::endl;
      return -1;
    }
    return kv_server::blob_storage_commands::ListObjects(std::move(bucket),
                                                         std::move(prefix))
               ? 0
               : -1;
  }
  if (operation == "rm") {
    if (commands.size() < 3) {
      std::cerr << "Did not take get key name(s)." << std::endl;
      return -1;
    }
    return kv_server::blob_storage_commands::DeleteObjects(
               std::move(bucket), std::move(prefix),
               absl::MakeSpan(commands).subspan(2))
               ? 0
               : -1;
  }
  if (operation == "cat") {
    if (commands.size() < 3) {
      std::cerr << "Did not take get key name(s)." << std::endl;
      return -1;
    }
    return kv_server::blob_storage_commands::CatObjects(
               std::move(bucket), std::move(prefix),
               absl::MakeSpan(commands).subspan(2))
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
