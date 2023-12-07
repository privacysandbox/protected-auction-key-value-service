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

#include <algorithm>
#include <cstddef>
#include <thread>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "components/data/common/thread_manager.h"
#include "components/data/realtime/delta_file_record_change_notifier.h"
#include "components/data/realtime/realtime_notifier.h"
#include "components/errors/retry.h"
#include "glog/logging.h"
#include "public/constants.h"
#include "src/cpp/telemetry/metrics_recorder.h"
#include "src/cpp/telemetry/telemetry.h"
#include "src/cpp/util/duration.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::MetricsRecorder;

constexpr char* kReceivedLowLatencyNotifications =
    "ReceivedLowLatencyNotifications";
constexpr char* kReceivedLowLatencyNotificationsE2E =
    "ReceivedLowLatencyNotificationsE2E";
constexpr char* kRealtimeTotalRowsUpdated = "RealtimeTotalRowsUpdated";
constexpr char* kReceivedLowLatencyNotificationsE2EAwsProvided =
    "ReceivedLowLatencyNotificationsE2EAwsProvided";
constexpr char* kRealtimeGetNotificationsFailure =
    "RealtimeGetNotificationsFailure";
constexpr char* kRealtimeSleepFailure = "RealtimeSleepFailure";

// The units below are microseconds.
const std::vector<double> kE2eBucketBoundaries = {
    160,     220,       280,       320,       640,       1'200,         2'500,
    5'000,   10'000,    20'000,    40'000,    80'000,    160'000,       320'000,
    640'000, 1'000'000, 1'300'000, 2'600'000, 5'000'000, 10'000'000'000};

class RealtimeNotifierImpl : public RealtimeNotifier {
 public:
  explicit RealtimeNotifierImpl(
      MetricsRecorder& metrics_recorder, std::unique_ptr<SleepFor> sleep_for,
      std::unique_ptr<DeltaFileRecordChangeNotifier> change_notifier)
      : thread_manager_(TheadManager::Create("Realtime notifier")),
        metrics_recorder_(metrics_recorder),
        sleep_for_(std::move(sleep_for)),
        change_notifier_(std::move(change_notifier)) {
    metrics_recorder.RegisterHistogram(kReceivedLowLatencyNotificationsE2E,
                                       "Low latency notifictionas E2E latency",
                                       "microsecond", kE2eBucketBoundaries);
    metrics_recorder.RegisterHistogram(
        kReceivedLowLatencyNotificationsE2EAwsProvided,
        "Low latency notifications E2E latency aws supplied", "microsecond",
        kE2eBucketBoundaries);
  }

  absl::Status Start(
      std::function<absl::StatusOr<DataLoadingStats>(const std::string& key)>
          callback) override {
    return thread_manager_->Start([this, &change_notifier = *change_notifier_,
                                   callback = std::move(callback)]() mutable {
      Watch(change_notifier, std::move(callback));
    });
  }

  absl::Status Stop() override {
    absl::Status status = sleep_for_->Stop();
    status.Update(thread_manager_->Stop());
    return status;
  }

  bool IsRunning() const override { return thread_manager_->IsRunning(); }

 private:
  void Watch(
      DeltaFileRecordChangeNotifier& change_notifier,
      std::function<absl::StatusOr<DataLoadingStats>(const std::string& key)>
          callback) {
    // Starts with zero wait to force an initial short poll.
    // Later polls are long polls.
    auto max_wait = absl::ZeroDuration();
    uint32_t sequential_failures = 0;
    while (!thread_manager_->ShouldStop()) {
      auto updates = change_notifier.GetNotifications(
          max_wait, [this]() { return thread_manager_->ShouldStop(); });

      if (absl::IsDeadlineExceeded(updates.status())) {
        sequential_failures = 0;
        max_wait = absl::InfiniteDuration();
        continue;
      }

      if (!updates.ok()) {
        ++sequential_failures;
        const absl::Duration backoff_time =
            ExponentialBackoffForRetry(sequential_failures);
        LOG(ERROR) << "Failed to get realtime notifications: "
                   << updates.status() << ".  Waiting for " << backoff_time;
        metrics_recorder_.IncrementEventCounter(
            kRealtimeGetNotificationsFailure);
        if (!sleep_for_->Duration(backoff_time)) {
          LOG(ERROR) << "Failed to sleep for " << backoff_time
                     << ".  SleepFor invalid.";
          metrics_recorder_.IncrementEventCounter(kRealtimeSleepFailure);
        }
        continue;
      }
      sequential_failures = 0;

      for (const auto& realtime_message : updates->realtime_messages) {
        auto count = callback(realtime_message.parsed_notification);
        if (count.ok()) {
          metrics_recorder_.IncrementEventStatus(
              kRealtimeTotalRowsUpdated, count.status(),
              (count->total_updated_records + count->total_deleted_records));
        }
        metrics_recorder_.RecordHistogramEvent(
            kReceivedLowLatencyNotificationsE2EAwsProvided,
            absl::ToInt64Microseconds(
                absl::Now() - (realtime_message.notifications_sns_inserted)));

        if (realtime_message.notifications_inserted) {
          auto e2eDuration =
              absl::Now() - (realtime_message.notifications_inserted).value();
          metrics_recorder_.RecordHistogramEvent(
              kReceivedLowLatencyNotificationsE2E,
              absl::ToInt64Microseconds(e2eDuration));
        }
      }

      metrics_recorder_.RecordLatency(
          kReceivedLowLatencyNotifications,
          (absl::Now() - updates->notifications_received));

      // if we don't move it here, then it will destroy this object
      // downstack, and the latency of the trace will be incorrect.
      auto low_latency_scope = std::move(updates->scope);
      max_wait = absl::InfiniteDuration();
    }
  }

  std::unique_ptr<TheadManager> thread_manager_;
  MetricsRecorder& metrics_recorder_;
  std::unique_ptr<SleepFor> sleep_for_;
  std::unique_ptr<DeltaFileRecordChangeNotifier> change_notifier_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<RealtimeNotifier>> RealtimeNotifier::Create(
    MetricsRecorder& metrics_recorder, NotifierMetadata notifier_metadata,
    RealtimeNotifierMetadata realtime_notifier_metadata) {
  auto options =
      std::get_if<AwsRealtimeNotifierMetadata>(&realtime_notifier_metadata);
  std::unique_ptr<DeltaFileRecordChangeNotifier>
      delta_file_record_change_notifier;
  if (options && options->change_notifier_for_unit_testing) {
    delta_file_record_change_notifier.reset(
        options->change_notifier_for_unit_testing);
  } else {
    auto status_or_notifier =
        ChangeNotifier::Create(std::move(notifier_metadata));
    if (!status_or_notifier.ok()) {
      return status_or_notifier.status();
    }
    delta_file_record_change_notifier = DeltaFileRecordChangeNotifier::Create(
        std::move(*status_or_notifier), metrics_recorder);
  }
  std::unique_ptr<SleepFor> sleep_for;
  if (options && options->maybe_sleep_for) {
    sleep_for = std::move(options->maybe_sleep_for);
  } else {
    sleep_for = std::make_unique<SleepFor>();
  }
  return std::make_unique<RealtimeNotifierImpl>(
      metrics_recorder, std::move(sleep_for),
      std::move(delta_file_record_change_notifier));
}

}  // namespace kv_server
