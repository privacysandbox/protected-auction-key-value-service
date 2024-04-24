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

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "components/data/common/thread_manager.h"
#include "components/data/realtime/delta_file_record_change_notifier.h"
#include "components/data/realtime/realtime_notifier.h"
#include "components/errors/retry.h"
#include "public/constants.h"
#include "src/telemetry/telemetry.h"
#include "src/util/duration.h"

namespace kv_server {
namespace {

class RealtimeNotifierImpl : public RealtimeNotifier {
 public:
  explicit RealtimeNotifierImpl(
      std::unique_ptr<SleepFor> sleep_for,
      std::unique_ptr<DeltaFileRecordChangeNotifier> change_notifier,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : thread_manager_(ThreadManager::Create("Realtime notifier")),
        sleep_for_(std::move(sleep_for)),
        change_notifier_(std::move(change_notifier)),
        log_context_(log_context) {}

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
        PS_LOG(ERROR, log_context_)
            << "Failed to get realtime notifications: " << updates.status()
            << ".  Waiting for " << backoff_time;
        LogServerErrorMetric(kRealtimeGetNotificationsFailure);
        if (!sleep_for_->Duration(backoff_time)) {
          PS_LOG(ERROR, log_context_) << "Failed to sleep for " << backoff_time
                                      << ".  SleepFor invalid.";
          LogServerErrorMetric(kRealtimeSleepFailure);
        }
        continue;
      }
      sequential_failures = 0;

      for (const auto& realtime_message : updates->realtime_messages) {
        if (auto count = callback(realtime_message.parsed_notification);
            !count.ok()) {
          PS_LOG(ERROR, log_context_)
              << "Data loading callback failed: " << count.status();
          LogServerErrorMetric(kRealtimeMessageApplicationFailure);
        }
        auto e2e_cloud_provided_latency = absl::ToDoubleMicroseconds(
            absl::Now() - realtime_message.notifications_sns_inserted);
        // we're getting this value based on two different clocks. Opentelemetry
        // does not allow negative values for histograms. However, not logging
        // this will affect the pvalues, so the next best thing is set it to 0.
        if (e2e_cloud_provided_latency < 0) {
          e2e_cloud_provided_latency = 0;
        }
        LogIfError(
            KVServerContextMap()
                ->SafeMetric()
                .LogHistogram<kReceivedLowLatencyNotificationsE2ECloudProvided>(
                    e2e_cloud_provided_latency));

        if (realtime_message.notifications_inserted) {
          // we're getting this value based on two different clocks.
          // Opentelemetry does not allow negative values for histograms.
          // However, not logging this will affect the pvalues, so the next best
          // thing is set it to 0.
          auto e2e_latency = absl::ToDoubleMicroseconds(
              absl::Now() - realtime_message.notifications_inserted.value());
          if (e2e_latency < 0) {
            e2e_latency = 0;
          }
          LogIfError(KVServerContextMap()
                         ->SafeMetric()
                         .LogHistogram<kReceivedLowLatencyNotificationsE2E>(
                             e2e_latency));
        }
      }
      LogIfError(KVServerContextMap()
                     ->SafeMetric()
                     .LogHistogram<kReceivedLowLatencyNotifications>(
                         absl::ToDoubleMicroseconds(
                             absl::Now() - updates->notifications_received)));
      // if we don't move it here, then it will destroy this object
      // downstack, and the latency of the trace will be incorrect.
      auto low_latency_scope = std::move(updates->scope);
      max_wait = absl::InfiniteDuration();
    }
  }

  std::unique_ptr<ThreadManager> thread_manager_;
  std::unique_ptr<SleepFor> sleep_for_;
  std::unique_ptr<DeltaFileRecordChangeNotifier> change_notifier_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<RealtimeNotifier>> RealtimeNotifier::Create(
    NotifierMetadata notifier_metadata,
    RealtimeNotifierMetadata realtime_notifier_metadata,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
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
    delta_file_record_change_notifier =
        DeltaFileRecordChangeNotifier::Create(std::move(*status_or_notifier));
  }
  std::unique_ptr<SleepFor> sleep_for;
  if (options && options->maybe_sleep_for) {
    sleep_for = std::move(options->maybe_sleep_for);
  } else {
    sleep_for = std::make_unique<SleepFor>();
  }
  return std::make_unique<RealtimeNotifierImpl>(
      std::move(sleep_for), std::move(delta_file_record_change_notifier),
      log_context);
}

}  // namespace kv_server
