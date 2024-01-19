// Copyright 2024 Google LLC
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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "components/tools/publisher_service.h"

namespace kv_server {
namespace {

class LocalPublisherService : public PublisherService {
 public:
  LocalPublisherService() {}

  absl::Status Publish(const std::string& body, std::optional<int> shard_num) {
    return absl::UnimplementedError(
        "PublisherService is not implemented for local");
  }

  absl::StatusOr<NotifierMetadata> BuildNotifierMetadataAndSetQueue() {
    return absl::UnimplementedError(
        "PublisherService is not implemented for local");
  }
};

}  // namespace

absl::StatusOr<std::unique_ptr<PublisherService>> PublisherService::Create(
    NotifierMetadata notifier_metadata) {
  return std::make_unique<LocalPublisherService>();
}

absl::StatusOr<NotifierMetadata> PublisherService::GetNotifierMetadata() {
  return absl::UnimplementedError(
      "PublisherService is not implemented for local");
}
}  // namespace kv_server
