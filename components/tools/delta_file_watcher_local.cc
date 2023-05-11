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

#include <filesystem>
#include <future>
#include <iostream>

#include <glog/logging.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/blob_storage/delta_file_notifier.h"
#include "components/data/common/change_notifier.h"
#include "components/data/common/thread_notifier.h"
#include "components/util/platform_initializer.h"
#include "src/cpp/telemetry/telemetry_provider.h"

ABSL_FLAG(std::string, directory, "", "Local directory to watch");

using kv_server::BlobStorageChangeNotifier;
using kv_server::BlobStorageClient;
using kv_server::DeltaFileNotifier;
using privacy_sandbox::server_common::TelemetryProvider;

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  kv_server::PlatformInitializer initializer;

  const std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  const std::string directory = absl::GetFlag(FLAGS_directory);
  if (directory.empty()) {
    std::cerr << "Must specify local directory" << std::endl;
    return -1;
  }
  auto noop_metrics_recorder =
      TelemetryProvider::GetInstance().CreateMetricsRecorder();
  std::unique_ptr<BlobStorageClient> client =
      BlobStorageClient::Create(*noop_metrics_recorder);
  std::unique_ptr<DeltaFileNotifier> notifier =
      DeltaFileNotifier::Create(*client);
  auto status_or_change_notifier = BlobStorageChangeNotifier::Create(
      kv_server::LocalNotifierMetadata{.local_directory =
                                           std::filesystem::path(directory)},
      *noop_metrics_recorder);

  if (!status_or_change_notifier.ok()) {
    std::cerr << "Unable to create BlobStorageChangeNotifier: "
              << status_or_change_notifier.status().message();
    return -1;
  }

  const absl::Status status = notifier->Start(
      **status_or_change_notifier, {.bucket = std::move(directory)}, "",
      [](const std::string& key) { std::cout << key << std::endl; });
  if (!status.ok()) {
    std::cerr << "Failed to start notifier: " << status << std::endl;
    return 1;
  }
  // Wait forever
  std::promise<void>().get_future().wait();
  return 0;
}
