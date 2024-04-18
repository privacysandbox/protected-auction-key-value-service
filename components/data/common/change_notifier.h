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

#ifndef COMPONENTS_DATA_COMMON_CHANGE_NOTIFIER_H_
#define COMPONENTS_DATA_COMMON_CHANGE_NOTIFIER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/data/common/msg_svc.h"
#include "src/logger/request_context_logger.h"

namespace kv_server {

// Change notifier enables to receive notifications about updates coming
// from the customer, e.g. delta file names uploaded to an s3 bucket, real time
// updates.
// Example:
//   auto change_notifier_ = ChangeNotifier::Create("BlobNotifier_", sns_arn);
//   change_notifier_->GetNotifications(
//         max_wait,
//         [this](const std::string& message_body) {return
//         ParseObjectKeyFromJson(message_body);}, should_stop_callback);
class ChangeNotifier {
 public:
  virtual ~ChangeNotifier() = default;

  virtual absl::StatusOr<std::vector<std::string>> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) = 0;

  static absl::StatusOr<std::unique_ptr<ChangeNotifier>> Create(
      NotifierMetadata notifier_metadata,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));
};

}  // namespace kv_server
#endif  // COMPONENTS_DATA_COMMON_CHANGE_NOTIFIER_H_
