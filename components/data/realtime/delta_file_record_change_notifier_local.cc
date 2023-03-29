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
#include "components/data/common/change_notifier.h"
#include "components/data/realtime/delta_file_record_change_notifier.h"
#include "glog/logging.h"

namespace kv_server {
namespace {

constexpr char* RECEIVED_LOW_LATENCY_NOTIFICATIONS_LOCALLY =
    "ReceivedLowLatencyNotificationsLocally";

class LocalDeltaFileRecordChangeNotifier
    : public DeltaFileRecordChangeNotifier {
 public:
  explicit LocalDeltaFileRecordChangeNotifier(
      std::unique_ptr<ChangeNotifier> notifier)
      : notifier_(std::move(notifier)) {}

  absl::StatusOr<NotificationsContext> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    auto notifications =
        notifier_->GetNotifications(max_wait, should_stop_callback);
    if (!notifications.ok()) {
      return notifications.status();
    }
    auto span =
        GetTracer()->StartSpan(RECEIVED_LOW_LATENCY_NOTIFICATIONS_LOCALLY);
    NotificationsContext nc = {
        .scope = opentelemetry::trace::Scope(span),
        .notifications_received = absl::Now(),
        .parsed_notifications = *notifications,
    };
    return nc;
  }

 private:
  std::unique_ptr<ChangeNotifier> notifier_;
};

}  // namespace

std::unique_ptr<DeltaFileRecordChangeNotifier>
DeltaFileRecordChangeNotifier::Create(
    std::unique_ptr<ChangeNotifier> change_notifier) {
  return std::make_unique<LocalDeltaFileRecordChangeNotifier>(
      std::move(change_notifier));
}

}  // namespace kv_server
