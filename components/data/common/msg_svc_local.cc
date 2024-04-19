// Copyright 2023 Google LLC
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

#include <optional>

#include "components/data/common/msg_svc.h"

namespace kv_server {
namespace {
class LocalMessageService : public MessageService {
 public:
  explicit LocalMessageService(
      std::string local_directory,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : log_context_(log_context) {}
  bool IsSetupComplete() const { return true; }
  const QueueMetadata GetQueueMetadata() const {
    AwsQueueMetadata metadata;
    return metadata;
  }
  absl::Status SetupQueue() { return absl::OkStatus(); }
  void Reset() {}
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<MessageService>> MessageService::Create(
    NotifierMetadata notifier_metadata,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  auto metadata = std::get<LocalNotifierMetadata>(notifier_metadata);
  return std::make_unique<LocalMessageService>(
      std::move(metadata.local_directory), log_context);
}
}  // namespace kv_server
