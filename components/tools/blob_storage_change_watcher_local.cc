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
// Example invocation:
// ./blob_storage_change_watcher --directory=/tmp
//
#include <filesystem>
#include <future>
#include <iostream>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "components/data/blob_storage/blob_storage_change_notifier.h"
#include "components/data/common/change_notifier.h"
#include "components/telemetry/telemetry_provider.h"
#include "components/util/platform_initializer.h"
#include "glog/logging.h"

ABSL_FLAG(std::string, directory, "", "Local directory to watch");

using kv_server::BlobStorageChangeNotifier;
using kv_server::TelemetryProvider;

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  kv_server::PlatformInitializer initializer;

  absl::ParseCommandLine(argc, argv);
  const std::string directory = absl::GetFlag(FLAGS_directory);
  if (directory.empty()) {
    std::cerr << "Must specify --directory" << std::endl;
    return -1;
  }

  auto noop_metrics_recorder =
      TelemetryProvider::GetInstance().CreateMetricsRecorder();
  auto status_or_notifier = BlobStorageChangeNotifier::Create(
      kv_server::LocalNotifierMetadata{.local_directory =
                                           std::filesystem::path(directory)},
      *noop_metrics_recorder);

  if (!status_or_notifier.ok()) {
    std::cerr << "Unable to create BlobStorageChangeNotifier: "
              << status_or_notifier.status().message();
    return -1;
  }

  while (true) {
    absl::StatusOr<std::vector<std::string>> keys =
        (*status_or_notifier)->GetNotifications(absl::InfiniteDuration(), []() {
          return false;
        });
    if (keys.ok()) {
      for (const auto& key : *keys) {
        std::cout << "Found new blob: " << key << std::endl;
      }
    } else {
      std::cerr << "Error watching directory: " << keys.status() << std::endl;
    }
  }
  return 0;
}
