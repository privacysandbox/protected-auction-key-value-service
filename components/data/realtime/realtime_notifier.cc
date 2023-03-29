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

#include "components/data/realtime/realtime_notifier.h"

#include <algorithm>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "components/data/common/thread_notifier.h"
#include "components/data/realtime/delta_file_record_change_notifier.h"
#include "components/errors/retry.h"
#include "components/telemetry/metrics_recorder.h"
#include "components/telemetry/telemetry.h"
#include "components/util/duration.h"
#include "glog/logging.h"
#include "public/constants.h"

namespace kv_server {
namespace {
constexpr char* kReceivedLowLatencyNotifications =
    "ReceivedLowLatencyNotifications";

constexpr char* kReceivedLowLatencyNotificationsE2E =
    "ReceivedLowLatencyNotificationsE2E";

// The units below are microseconds.
const std::vector<double> kE2eBucketBoundaries = {
    160,     220,       280,       320,       640,       1'200,         2'500,
    5'000,   10'000,    20'000,    40'000,    80'000,    160'000,       320'000,
    640'000, 1'000'000, 1'300'000, 2'600'000, 5'000'000, 10'000'000'000};

class RealtimeNotifierImpl : public RealtimeNotifier {
 public:
  explicit RealtimeNotifierImpl(MetricsRecorder& metrics_recorder,
                                std::unique_ptr<SleepFor> sleep_for)
      : thread_notifier_(ThreadNotifier::Create("Realtime notifier")),
        metrics_recorder_(metrics_recorder),
        sleep_for_(std::move(sleep_for)) {
    metrics_recorder.RegisterHistogram(kReceivedLowLatencyNotificationsE2E,
                                       "Low latency notifictionas E2E latency",
                                       "microsecond", kE2eBucketBoundaries);
  }

  absl::Status Start(
      DeltaFileRecordChangeNotifier& change_notifier,
      std::function<void(const std::string& key)> callback) override {
    return thread_notifier_->Start(
        [this, callback = std::move(callback), &change_notifier]() mutable {
          Watch(change_notifier, std::move(callback));
        });
  }

  absl::Status Stop() override {
    absl::Status status = sleep_for_->Stop();
    status.Update(thread_notifier_->Stop());
    return status;
  }

  bool IsRunning() const override { return thread_notifier_->IsRunning(); }

 private:
  void Watch(DeltaFileRecordChangeNotifier& change_notifier,
             std::function<void(const std::string& key)> callback) {
    // Starts with zero wait to force an initial short poll.
    // Later polls are long polls.
    auto max_wait = absl::ZeroDuration();
    uint32_t sequential_failures = 0;
    while (!thread_notifier_->ShouldStop()) {
      auto updates = change_notifier.GetNotifications(
          max_wait, [this]() { return thread_notifier_->ShouldStop(); });

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
        if (!sleep_for_->Duration(backoff_time)) {
          LOG(ERROR) << "Failed to sleep for " << backoff_time
                     << ".  SleepFor invalid.";
        }
        continue;
      }
      sequential_failures = 0;

      for (const std::string& key : updates->parsed_notifications) {
        callback(key);
      }

      metrics_recorder_.RecordLatency(
          kReceivedLowLatencyNotifications,
          (absl::Now() - updates->notifications_received));
      if (updates->notifications_inserted) {
        auto e2eDuration =
            absl::Now() - (updates->notifications_inserted).value();
        metrics_recorder_.RecordHistogramEvent(
            kReceivedLowLatencyNotificationsE2E,
            absl::ToInt64Microseconds(e2eDuration));
      }

      // if we don't move it here, then it will destroy this object
      // downstack, and the latency of the trace will be incorrect.
      auto low_latency_scope = std::move(updates->scope);
      max_wait = absl::InfiniteDuration();
    }
  }

  std::unique_ptr<ThreadNotifier> thread_notifier_;
  MetricsRecorder& metrics_recorder_;
  std::unique_ptr<SleepFor> sleep_for_;
};

}  // namespace

std::unique_ptr<RealtimeNotifier> RealtimeNotifier::Create(
    MetricsRecorder& metrics_recorder) {
  return std::make_unique<RealtimeNotifierImpl>(metrics_recorder,
                                                std::make_unique<SleepFor>());
}

// Used for test
std::unique_ptr<RealtimeNotifier> RealtimeNotifier::Create(
    MetricsRecorder& metrics_recorder, std::unique_ptr<SleepFor> sleep_for) {
  return std::make_unique<RealtimeNotifierImpl>(metrics_recorder,
                                                std::move(sleep_for));
}

}  // namespace kv_server
