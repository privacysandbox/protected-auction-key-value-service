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
#include <future>
#include <iostream>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "components/data/blob_storage/blob_storage_change_notifier.h"
#include "components/telemetry/server_definition.h"
#include "components/tools/util/configure_telemetry_tools.h"
#include "components/util/platform_initializer.h"
#include "src/telemetry/telemetry_provider.h"

ABSL_FLAG(std::string, sns_arn, "", "sns_arn");

using kv_server::BlobStorageChangeNotifier;
using privacy_sandbox::server_common::TelemetryProvider;

int main(int argc, char** argv) {
  kv_server::PlatformInitializer initializer;

  std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  std::string sns_arn = absl::GetFlag(FLAGS_sns_arn);
  if (sns_arn.empty()) {
    std::cerr << "Must specify sns_arn" << std::endl;
    return -1;
  }
  // Initialize no-op telemetry
  kv_server::ConfigureTelemetryForTools();
  auto message_service_status = kv_server::MessageService::Create(
      kv_server::AwsNotifierMetadata{"BlobNotifier_", sns_arn});

  if (!message_service_status.ok()) {
    std::cerr << "Unable to create MessageService: "
              << message_service_status.status().message();
    return -1;
  }

  auto status_or_notifier =
      BlobStorageChangeNotifier::Create(kv_server::AwsNotifierMetadata{
          .sns_arn = sns_arn, .queue_manager = message_service_status->get()});

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
        std::cout << key << std::endl;
      }
    } else {
      std::cerr << keys.status() << std::endl;
    }
  }
  return 0;
}
