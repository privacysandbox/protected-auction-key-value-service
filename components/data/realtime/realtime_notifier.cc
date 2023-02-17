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
#include "components/util/duration.h"
#include "glog/logging.h"
#include "public/constants.h"

namespace kv_server {
namespace {

class RealtimeNotifierImpl : public RealtimeNotifier {
 public:
  explicit RealtimeNotifierImpl(ThreadNotifier& thread_notifier,
                                const SleepFor& sleep_for)
      : thread_notifier_(thread_notifier), sleep_for_(sleep_for) {}

  absl::Status Start(
      DeltaFileRecordChangeNotifier& change_notifier,
      std::function<void(const std::string& key)> callback) override {
    return thread_notifier_.Start(
        [this, callback = std::move(callback), &change_notifier]() mutable {
          Watch(change_notifier, std::move(callback));
        });
  }

  absl::Status Stop() override { return thread_notifier_.Stop(); }

  bool IsRunning() const override { return thread_notifier_.IsRunning(); }

 private:
  void Watch(DeltaFileRecordChangeNotifier& change_notifier,
             std::function<void(const std::string& key)> callback) {
    // Starts with zero wait to force an initial short poll.
    // Later polls are long polls.
    auto max_wait = absl::ZeroDuration();
    uint32_t sequential_failures = 0;
    while (!thread_notifier_.ShouldStop()) {
      const absl::StatusOr<std::vector<std::string>> updates =
          change_notifier.GetNotifications(
              max_wait, [this]() { return thread_notifier_.ShouldStop(); });

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
        // TODO: b/268557235,  SleepFor needs to be interruptable
        sleep_for_.Duration(backoff_time);
        continue;
      }
      sequential_failures = 0;

      for (const std::string& key : *updates) {
        callback(key);
      }

      max_wait = absl::InfiniteDuration();
    }
  }

  ThreadNotifier& thread_notifier_;
  const SleepFor& sleep_for_;
};

}  // namespace

std::unique_ptr<RealtimeNotifier> RealtimeNotifier::Create(
    ThreadNotifier& thread_notifier, const SleepFor& sleep_for) {
  return std::make_unique<RealtimeNotifierImpl>(thread_notifier, sleep_for);
}

}  // namespace kv_server
