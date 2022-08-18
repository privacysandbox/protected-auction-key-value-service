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
#include "components/data/blob_storage_change_notifier.h"

ABSL_FLAG(std::string, sns_arn, "", "sns_arn");
using namespace fledge::kv_server;

int main(int argc, char** argv) {
  // TODO: use cc/cpio/cloud_providers to initialize cloud.
  Aws::SDKOptions options;
  Aws::InitAPI(options);
  absl::Cleanup shutdown = [&options] { Aws::ShutdownAPI(options); };

  std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  std::string sns_arn = absl::GetFlag(FLAGS_sns_arn);
  if (sns_arn.empty()) {
    std::cerr << "Must specify sns_arn" << std::endl;
    return -1;
  }
  std::unique_ptr<BlobStorageChangeNotifier> notifier =
      BlobStorageChangeNotifier::Create({.sns_arn = sns_arn});

  while (true) {
    absl::StatusOr<std::vector<std::string>> keys = notifier->GetNotifications(
        absl::InfiniteDuration(), []() { return false; });
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
