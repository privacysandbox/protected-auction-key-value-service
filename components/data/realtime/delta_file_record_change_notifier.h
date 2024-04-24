/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_DATA_REALTIME_DELTA_FILE_RECORD_NOTIFIER_H_
#define COMPONENTS_DATA_REALTIME_DELTA_FILE_RECORD_NOTIFIER_H_

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "components/data/common/change_notifier.h"
#include "src/telemetry/telemetry.h"

namespace kv_server {

struct RealtimeMessage {
  std::string parsed_notification;

  // Time set producer side before the message was inserted to the pubsub.
  // since we read a batch of messages from the queue, this will aways be the
  // min of all insertion times. Note that for most usecases this will be null,
  // as it is only used for measuring latency during tests.
  // Note that since the time differences are so small, the clock skew might
  // affect the result. So you should be sending your requests from the same
  // machine on which the server is running -- i.e. cloudtop.
  std::optional<absl::Time> notifications_inserted;
  absl::Time notifications_sns_inserted;
};

struct NotificationsContext {
  //  A list of messages passed through the realtime endpoint in a single call
  std::vector<RealtimeMessage> realtime_messages;
  // The time when these notifications were received from server side
  absl::Time notifications_received;
  // An open telemetry scope assosiated with the beggining of the trace that
  // started when these notifications were received.
  opentelemetry::trace::Scope scope;
};

// This notifier enables receiving updates from an SNS.
// Example:
//   auto notifier_ = DeltaFileRecordChangeNotifier::Create(change_notifier);
//   notifier_->GetNotifications(max_wait, should_stop_callback);
class DeltaFileRecordChangeNotifier {
 public:
  virtual ~DeltaFileRecordChangeNotifier() = default;

  // Waits up to `max_wait` to return a vector of realtime notifications.
  // `should_stop_callback` is called periodically as a signal to abort prior to
  // `max_wait`.
  virtual absl::StatusOr<NotificationsContext> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) = 0;

  static std::unique_ptr<DeltaFileRecordChangeNotifier> Create(
      std::unique_ptr<ChangeNotifier> change_notifier,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_REALTIME_DELTA_FILE_RECORD_NOTIFIER_H_
