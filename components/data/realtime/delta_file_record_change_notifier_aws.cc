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

#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "aws/core/utils/json/JsonSerializer.h"
#include "components/data/common/change_notifier.h"
#include "components/data/realtime/delta_file_record_change_notifier.h"
#include "components/telemetry/telemetry.h"
#include "glog/logging.h"

namespace kv_server {
namespace {

constexpr char* RECEIVED_LOW_LATENCY_NOTIFICATIONS =
    "ReceivedLowLatencyNotifications";

struct ParsedBody {
  std::string message;
  std::optional<absl::Time> time_sent;
};

class AwsDeltaFileRecordChangeNotifier : public DeltaFileRecordChangeNotifier {
 public:
  explicit AwsDeltaFileRecordChangeNotifier(
      std::unique_ptr<ChangeNotifier> change_notifier)
      : change_notifier_(std::move(change_notifier)) {}

  absl::StatusOr<NotificationsContext> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    auto notifications =
        change_notifier_->GetNotifications(max_wait, should_stop_callback);

    if (!notifications.ok()) {
      return notifications.status();
    }

    auto span = GetTracer()->StartSpan(RECEIVED_LOW_LATENCY_NOTIFICATIONS);
    NotificationsContext nc = {.scope = opentelemetry::trace::Scope(span),
                               .notifications_received = absl::Now()};
    std::vector<std::string> parsed_notifications;
    for (const auto& message : *notifications) {
      const auto parsedMessage = ParseObjectKeyFromJson(message);
      if (!parsedMessage.ok()) {
        LOG(ERROR) << "Failed to parse JSON: " << message;
        continue;
      }
      nc.parsed_notifications.push_back(std::move(parsedMessage->message));

      if (!parsedMessage->time_sent) {
        continue;
      }
      if (nc.notifications_inserted &&
          nc.notifications_inserted <= parsedMessage->time_sent) {
        continue;
      }

      // only update if the value has been passed and it hasn't been set before
      // or it's smaller then the current value
      nc.notifications_inserted = parsedMessage->time_sent;
    }
    return nc;
  }

 private:
  absl::StatusOr<ParsedBody> ParseObjectKeyFromJson(const std::string& body) {
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
    ParsedBody pb = {.message = string_decoded};
    const auto message_attributes = view.GetObject("MessageAttributes");
    if (!message_attributes.IsObject()) {
      return pb;
    }
    const auto time_sent = message_attributes.GetObject("time_sent");
    if (!time_sent.IsObject() || !time_sent.ValueExists("Value")) {
      return pb;
    }
    int64_t value_int64;
    if (!absl::SimpleAtoi(time_sent.GetObject("Value").AsString(),
                          &value_int64)) {
      return pb;
    }
    pb.time_sent = absl::FromUnixNanos(value_int64);
    return pb;
  }

  std::unique_ptr<ChangeNotifier> change_notifier_;
};
}  // namespace

std::unique_ptr<DeltaFileRecordChangeNotifier>
DeltaFileRecordChangeNotifier::Create(
    std::unique_ptr<ChangeNotifier> change_notifier) {
  return std::make_unique<AwsDeltaFileRecordChangeNotifier>(
      std::move(change_notifier));
}

}  // namespace kv_server
