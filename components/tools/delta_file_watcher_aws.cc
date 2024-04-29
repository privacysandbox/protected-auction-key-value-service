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
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/blob_storage/delta_file_notifier.h"
#include "components/data/common/thread_manager.h"
#include "components/tools/util/configure_telemetry_tools.h"
#include "components/util/platform_initializer.h"
#include "src/telemetry/telemetry_provider.h"

ABSL_FLAG(std::string, bucket, "", "cloud storage bucket name");
ABSL_FLAG(std::string, sns_arn, "", "sns_arn");

using kv_server::BlobStorageChangeNotifier;
using kv_server::BlobStorageClient;
using kv_server::BlobStorageClientFactory;
using kv_server::DeltaFileNotifier;
using privacy_sandbox::server_common::TelemetryProvider;

int main(int argc, char** argv) {
  kv_server::PlatformInitializer initializer;

  const std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  const std::string bucket = absl::GetFlag(FLAGS_bucket);
  if (bucket.empty()) {
    std::cerr << "Must specify bucket" << std::endl;
    return -1;
  }
  const std::string sns_arn = absl::GetFlag(FLAGS_sns_arn);
  if (sns_arn.empty()) {
    std::cerr << "Must specify sns_arn" << std::endl;
    return -1;
  }
  kv_server::ConfigureTelemetryForTools();
  std::unique_ptr<BlobStorageClientFactory> blob_storage_client_factory =
      BlobStorageClientFactory::Create();
  std::unique_ptr<BlobStorageClient> client =
      blob_storage_client_factory->CreateBlobStorageClient();
  std::unique_ptr<DeltaFileNotifier> notifier =
      DeltaFileNotifier::Create(*client);

  auto maybe_message_service = kv_server::MessageService::Create(
      kv_server::AwsNotifierMetadata{"BlobNotifier_", sns_arn});

  if (!maybe_message_service.ok()) {
    std::cerr << "Unable to create MessageService: "
              << maybe_message_service.status().message();
    return -1;
  }

  auto status_or_change_notifier =
      BlobStorageChangeNotifier::Create(kv_server::AwsNotifierMetadata{
          .sns_arn = sns_arn, .queue_manager = maybe_message_service->get()});

  if (!status_or_change_notifier.ok()) {
    std::cerr << "Unable to create BlobStorageChangeNotifier: "
              << status_or_change_notifier.status().message();
    return -1;
  }

  const absl::Status status = notifier->Start(
      **status_or_change_notifier, {.bucket = std::move(bucket)},
      /*prefix_start_after_map=*/{std::make_pair("", "")},
      [](const std::string& key) { std::cout << key << std::endl; });
  if (!status.ok()) {
    std::cerr << "Failed to start notifier: " << status << std::endl;
    return 1;
  }
  // Wait forever
  std::promise<void>().get_future().wait();
  return 0;
}
