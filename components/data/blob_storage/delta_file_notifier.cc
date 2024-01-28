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

#include "components/data/blob_storage/delta_file_notifier.h"

#include <algorithm>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "components/data/common/thread_manager.h"
#include "components/errors/retry.h"
#include "public/constants.h"
#include "public/data_loading/filename_utils.h"
#include "src/cpp/util/duration.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::ExpiringFlag;
using privacy_sandbox::server_common::SteadyClock;

class DeltaFileNotifierImpl : public DeltaFileNotifier {
 public:
  explicit DeltaFileNotifierImpl(BlobStorageClient& client,
                                 const absl::Duration poll_frequency,
                                 std::unique_ptr<SleepFor> sleep_for,
                                 SteadyClock& clock)
      : thread_manager_(TheadManager::Create("Delta file notifier")),
        client_(client),
        poll_frequency_(poll_frequency),
        sleep_for_(std::move(sleep_for)),
        clock_(clock) {}

  absl::Status Start(
      BlobStorageChangeNotifier& change_notifier,
      BlobStorageClient::DataLocation location, std::string start_after,
      std::function<void(const std::string& key)> callback) override {
    return thread_manager_->Start([this, location = std::move(location),
                                   start_after = std::move(start_after),
                                   callback = std::move(callback),
                                   &change_notifier]() mutable {
      Watch(change_notifier, std::move(location), std::move(start_after),
            std::move(callback));
    });
  }

  absl::Status Stop() override {
    absl::Status status = sleep_for_->Stop();
    status.Update(thread_manager_->Stop());
    return status;
  }

  bool IsRunning() const override { return thread_manager_->IsRunning(); }

 private:
  // Returns max DeltaFile in alphabetical order from notification
  // Empty string if wait_duration exceeded.
  absl::StatusOr<std::string> WaitForNotification(
      BlobStorageChangeNotifier& change_notifier,
      absl::Duration wait_duration) {
    absl::StatusOr<std::vector<std::string>> changes =
        change_notifier.GetNotifications(
            wait_duration, [this]() { return thread_manager_->ShouldStop(); });
    if (!changes.ok()) {
      return changes.status();
    }
    std::string_view max_change = "";
    for (const auto& change : *changes) {
      if (change > max_change && IsDeltaFilename(change)) {
        max_change = change;
      }
    }
    return std::string(max_change);
  }

  // Returns true if we need to check for new blobs.  Returns the error on
  // failure.
  absl::StatusOr<bool> ShouldListBlobs(
      BlobStorageChangeNotifier& change_notifier, ExpiringFlag& expiring_flag,
      std::string_view last_key) {
    if (!expiring_flag.Get()) {
      VLOG(5) << "Backup poll";
      return true;
    }
    absl::StatusOr<std::string> notification_key =
        WaitForNotification(change_notifier, expiring_flag.GetTimeRemaining());
    if (notification_key.ok() && *notification_key < last_key) {
      // Ignore notifications for keys we've already seen.
      return false;
    }
    if (absl::IsDeadlineExceeded(notification_key.status())) {
      // Deadline exceeded while waiting, trigger backup poll
      VLOG(5) << "Backup poll";
      return true;
    }
    // Don't poll on error.  A backup poll will trigger if necessary.
    if (!notification_key.ok()) {
      return notification_key.status();
    }
    // Return True if there is a Delta file notification
    // False is returned on DeadlineExceeded.
    return IsDeltaFilename(*notification_key);
  }

  void Watch(BlobStorageChangeNotifier& change_notifier,
             BlobStorageClient::DataLocation location, std::string start_after,
             std::function<void(const std::string& key)> callback) {
    LOG(INFO) << "Started to watch " << location;
    std::string last_key = std::move(start_after);
    // Flag starts expired, and forces an initial poll.
    ExpiringFlag expiring_flag(clock_);
    uint32_t sequential_failures = 0;
    while (!thread_manager_->ShouldStop()) {
      const absl::StatusOr<bool> should_list_blobs =
          ShouldListBlobs(change_notifier, expiring_flag, last_key);
      if (!should_list_blobs.ok()) {
        ++sequential_failures;
        const absl::Duration backoff_time =
            std::min(expiring_flag.GetTimeRemaining(),
                     ExponentialBackoffForRetry(sequential_failures));
        LOG(ERROR) << "Failed to get delta file notifications: "
                   << should_list_blobs.status() << ".  Waiting for "
                   << backoff_time;
        if (!sleep_for_->Duration(backoff_time)) {
          LOG(ERROR) << "Failed to sleep for " << backoff_time
                     << ".  SleepFor invalid.";
        }
        continue;
      }
      sequential_failures = 0;
      if (!*should_list_blobs) {
        continue;
      }
      absl::StatusOr<std::vector<std::string>> result = client_.ListBlobs(
          location, {.prefix = std::string(FilePrefix<FileType::DELTA>()),
                     .start_after = last_key});
      if (!result.ok()) {
        LOG(ERROR) << "Failed to list " << location << ": " << result.status();
        continue;
      }
      // Set expiring flag before callback for unit test simplicity.
      // Fake clock is moved forward in callback so flag must be set beforehand.
      expiring_flag.Set(poll_frequency_);
      int delta_file_count = 0;
      for (const std::string& key : *result) {
        if (!IsDeltaFilename(key)) {
          continue;
        }
        callback(key);
        last_key = key;
        delta_file_count++;
      }
      if (!delta_file_count) {
        VLOG(2) << "No new file found";
      }
    }
  }

  std::unique_ptr<TheadManager> thread_manager_;
  BlobStorageClient& client_;
  const absl::Duration poll_frequency_;
  std::unique_ptr<SleepFor> sleep_for_;
  SteadyClock& clock_;
};

}  // namespace

std::unique_ptr<DeltaFileNotifier> DeltaFileNotifier::Create(
    BlobStorageClient& client, const absl::Duration poll_frequency) {
  return std::make_unique<DeltaFileNotifierImpl>(client, poll_frequency,
                                                 std::make_unique<SleepFor>(),
                                                 SteadyClock::RealClock());
}

// For test only
std::unique_ptr<DeltaFileNotifier> DeltaFileNotifier::Create(
    BlobStorageClient& client, const absl::Duration poll_frequency,
    std::unique_ptr<SleepFor> sleep_for, SteadyClock& clock) {
  return std::make_unique<DeltaFileNotifierImpl>(client, poll_frequency,
                                                 std::move(sleep_for), clock);
}

}  // namespace kv_server
