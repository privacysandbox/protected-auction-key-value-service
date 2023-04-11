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
//
// Example invocations:
// $./blob_storage_util --directory=/tmp ls
// $./blob_storage_util --directory=/tmp cat one two three
// $./blob_storage_util --directory=/tmp rm one
//
#include <iostream>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/telemetry/telemetry_provider.h"
#include "components/tools/blob_storage_commands.h"
#include "components/util/platform_initializer.h"
#include "glog/logging.h"

ABSL_FLAG(std::string, directory, "", "Local directory to read from");

namespace {

using kv_server::BlobReader;
using kv_server::BlobStorageClient;
using kv_server::TelemetryProvider;

// Source and Destination files can be local files, and stdin ("-").
bool CpObjects(std::string directory, std::string source, std::string dest) {
  auto noop_metrics_recorder =
      TelemetryProvider::GetInstance().CreateMetricsRecorder();
  std::unique_ptr<BlobStorageClient> client =
      BlobStorageClient::Create(*noop_metrics_recorder);
  absl::StatusOr<std::unique_ptr<BlobReader>> reader =
      kv_server::blob_storage_commands::GetStdinOrFileSourceStream(
          directory, std::move(source));
  if (!reader.ok()) {
    std::cerr << "Unable to read input: " << reader.status() << std::endl;
    return false;
  }

  return kv_server::blob_storage_commands::
      CopyFromReaderToStdoutOrFileDestination(std::move(*reader),
                                              std::move(directory), dest);
}

}  // namespace

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  kv_server::PlatformInitializer initializer;

  absl::SetProgramUsageMessage("[cat|cp|ls|rm]");
  std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  if (commands.size() < 2) {
    std::cerr << "Must specify command: " << absl::ProgramUsageMessage()
              << std::endl;
    return -1;
  }
  std::string directory = absl::GetFlag(FLAGS_directory);
  if (!directory.size()) {
    std::cerr << "Must specify bucket" << std::endl;
    return -1;
  }
  const absl::string_view operation = commands[1];
  if (operation == "ls") {
    if (commands.size() != 2) {
      std::cerr << "ls does not take any extra arguments." << std::endl;
      return -1;
    }
    return kv_server::blob_storage_commands::ListObjects(std::move(directory))
               ? 0
               : -1;
  }
  if (operation == "rm") {
    if (commands.size() < 3) {
      std::cerr << "Did not take get key name(s)." << std::endl;
      return -1;
    }
    return kv_server::blob_storage_commands::DeleteObjects(
               std::move(directory), absl::MakeSpan(commands).subspan(2))
               ? 0
               : -1;
  }
  if (operation == "cat") {
    if (commands.size() < 3) {
      std::cerr << "Did not take get key name(s)." << std::endl;
      return -1;
    }
    return kv_server::blob_storage_commands::CatObjects(
               std::move(directory), absl::MakeSpan(commands).subspan(2))
               ? 0
               : -1;
  }
  if (operation == "cp") {
    if (commands.size() != 4) {
      std::cerr << "Expected args [source] [destination]" << std::endl;
      return -1;
    }
    return CpObjects(std::move(directory), commands[2], commands[3]) ? 0 : -1;
  }
  return 0;
}
