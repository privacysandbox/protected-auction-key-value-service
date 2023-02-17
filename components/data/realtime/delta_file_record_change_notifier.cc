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

#include "components/data/realtime/delta_file_record_change_notifier.h"

#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "aws/core/utils/json/JsonSerializer.h"
#include "components/data/common/change_notifier.h"
#include "glog/logging.h"

namespace kv_server {
namespace {

class AwsDeltaFileRecordChangeNotifier : public DeltaFileRecordChangeNotifier {
 public:
  explicit AwsDeltaFileRecordChangeNotifier(std::string sns_arn)
      : change_notifier_(ChangeNotifier::Create("QueueNotifier_", sns_arn)) {}

  absl::StatusOr<std::vector<std::string>> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    auto notifications =
        change_notifier_->GetNotifications(max_wait, should_stop_callback);

    if (!notifications.ok()) {
      return notifications.status();
    }

    std::vector<std::string> parsed_notifications;
    for (const auto& message : *notifications) {
      const absl::StatusOr<std::string> parsedMessage =
          ParseObjectKeyFromJson(message);
      if (!parsedMessage.ok()) {
        LOG(ERROR) << "Failed to parse JSON: " << message;
        continue;
      }
      parsed_notifications.push_back(std::move(*parsedMessage));
    }
    return parsed_notifications;
  }

 private:
  absl::StatusOr<std::string> ParseObjectKeyFromJson(const std::string& body) {
    Aws::Utils::Json::JsonValue json(body);
    if (!json.WasParseSuccessful()) {
      return absl::InvalidArgumentError(json.GetErrorMessage());
    }
    Aws::Utils::Json::JsonView view = json;
    const auto message = view.GetObject("Message");
    if (!message.IsString()) {
      return absl::InvalidArgumentError("Message is not a string");
    }

    std::string string_decoded;
    if (!absl::Base64Unescape(message.AsString(), &string_decoded)) {
      return absl::InvalidArgumentError(
          "The body of the message is not a base64 encoded string.");
    }

    return string_decoded;
  }

  std::unique_ptr<ChangeNotifier> change_notifier_;
};
}  // namespace

std::unique_ptr<DeltaFileRecordChangeNotifier>
DeltaFileRecordChangeNotifier::Create(NotifierMetadata metadata) {
  return std::make_unique<AwsDeltaFileRecordChangeNotifier>(
      std::move(metadata.sns_arn));
}

}  // namespace kv_server
