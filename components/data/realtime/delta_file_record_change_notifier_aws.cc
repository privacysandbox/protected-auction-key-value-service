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

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "aws/core/utils/json/JsonSerializer.h"
#include "components/data/common/change_notifier.h"
#include "components/data/realtime/delta_file_record_change_notifier.h"
#include "components/telemetry/server_definition.h"
#include "src/telemetry/telemetry.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::GetTracer;

constexpr char* kReceivedLowLatencyNotifications =
    "ReceivedLowLatencyNotifications";
constexpr char* kDeltaFileRecordChangeNotifierParsingFailure =
    "DeltaFileRecordChangeNotifierParsingFailure";

struct ParsedBody {
  std::string message;
  std::optional<absl::Time> time_sent;
  absl::Time time_sns_inserted;
};

class AwsDeltaFileRecordChangeNotifier : public DeltaFileRecordChangeNotifier {
 public:
  explicit AwsDeltaFileRecordChangeNotifier(
      std::unique_ptr<ChangeNotifier> change_notifier,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : change_notifier_(std::move(change_notifier)),
        log_context_(log_context) {}

  absl::StatusOr<NotificationsContext> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    auto notifications =
        change_notifier_->GetNotifications(max_wait, should_stop_callback);

    if (!notifications.ok()) {
      return notifications.status();
    }

    auto span = GetTracer()->StartSpan(kReceivedLowLatencyNotifications);
    NotificationsContext nc = {.scope = opentelemetry::trace::Scope(span),
                               .notifications_received = absl::Now()};
    std::vector<std::string> realtime_messages;
    for (const auto& message : *notifications) {
      const auto parsedMessage = ParseObjectKeyFromJson(message);
      if (!parsedMessage.ok()) {
        PS_LOG(ERROR, log_context_) << "Failed to parse JSON: " << message
                                    << ", error: " << parsedMessage.status();
        LogServerErrorMetric(kDeltaFileRecordChangeNotifierParsingFailure);
        continue;
      }
      nc.realtime_messages.push_back(RealtimeMessage{
          .parsed_notification = std::move(parsedMessage->message),
          .notifications_inserted = parsedMessage->time_sent,
          .notifications_sns_inserted = parsedMessage->time_sns_inserted});
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
    const auto sns_time_stamp = view.GetObject("Timestamp").AsString();

    std::string sns_time_stamp_parsin_error;
    absl::Time parsed_sns_timestamp;
    if (!absl::ParseTime(absl::RFC3339_full, sns_time_stamp,
                         absl::UTCTimeZone(), &parsed_sns_timestamp,
                         &sns_time_stamp_parsin_error)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Can't parse the time specified by sns_time_stamp: ", sns_time_stamp,
          ";error: ", sns_time_stamp_parsin_error));
    }

    pb.time_sns_inserted = parsed_sns_timestamp;

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
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};
}  // namespace

std::unique_ptr<DeltaFileRecordChangeNotifier>
DeltaFileRecordChangeNotifier::Create(
    std::unique_ptr<ChangeNotifier> change_notifier,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<AwsDeltaFileRecordChangeNotifier>(
      std::move(change_notifier), log_context);
}

}  // namespace kv_server
