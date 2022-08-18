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

#include "absl/cleanup/cleanup.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "aws/core/Aws.h"
#include "components/data/blob_storage_client.h"
#include "components/data/delta_file_notifier.h"

ABSL_FLAG(std::string, bucket, "", "cloud storage bucket name");
ABSL_FLAG(std::string, sns_arn, "", "sns_arn");

using namespace fledge::kv_server;

int main(int argc, char** argv) {
  // TODO: use cc/cpio/cloud_providers to initialize cloud.
  Aws::SDKOptions options;
  Aws::InitAPI(options);
  absl::Cleanup shutdown = [&options] { Aws::ShutdownAPI(options); };

  const std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  const std::string bucket = absl::GetFlag(FLAGS_bucket);
  if (!bucket.size()) {
    std::cerr << "Must specify bucket" << std::endl;
    return -1;
  }
  const std::string sns_arn = absl::GetFlag(FLAGS_sns_arn);
  if (sns_arn.empty()) {
    std::cerr << "Must specify sns_arn" << std::endl;
    return -1;
  }
  std::unique_ptr<BlobStorageClient> client = BlobStorageClient::Create();
  std::unique_ptr<DeltaFileNotifier> notifier =
      DeltaFileNotifier::Create(*client);
  std::unique_ptr<BlobStorageChangeNotifier> change_notifier =
      BlobStorageChangeNotifier::Create({.sns_arn = sns_arn});

  const absl::Status status = notifier->StartNotify(
      *change_notifier, {.bucket = std::move(bucket)}, "",
      [](const std::string& key) { std::cout << key << std::endl; });
  if (!status.ok()) {
    std::cerr << "Failed to start notifier: " << status << std::endl;
    return 1;
  }
  // Wait forever
  std::promise<void>().get_future().wait();
  return 0;
}
