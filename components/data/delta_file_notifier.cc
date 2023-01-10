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

#include "components/data/delta_file_notifier.h"

#include <algorithm>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "components/errors/retry.h"
#include "components/util/duration.h"
#include "glog/logging.h"
#include "public/constants.h"

namespace kv_server {
namespace {

class DeltaFileNotifierImpl : public DeltaFileNotifier {
 public:
  explicit DeltaFileNotifierImpl(BlobStorageClient& client,
                                 const absl::Duration poll_frequency,
                                 const SleepFor& sleep_for)
      : client_(client),
        poll_frequency_(poll_frequency),
        sleep_for_(sleep_for) {}

  ~DeltaFileNotifierImpl() {
    if (!IsRunning()) return;
    if (const auto s = StopNotify(); !s.ok()) {
      LOG(ERROR) << "Failed to stop delta file notifier: " << s;
    }
  }

  absl::Status StartNotify(
      BlobStorageChangeNotifier& change_notifier,
      BlobStorageClient::DataLocation location, std::string start_after,
      std::function<void(const std::string& key)> callback) override {
    if (IsRunning()) {
      return absl::FailedPreconditionError("Already notifying");
    }
    LOG(INFO) << "Creating thread for watching files";
    thread_ = std::make_unique<std::thread>(
        [this, location = std::move(location),
         start_after = std::move(start_after), callback = std::move(callback),
         &change_notifier]() mutable {
          Watch(change_notifier, std::move(location), std::move(start_after),
                std::move(callback));
        });
    return absl::OkStatus();
  }

  absl::Status StopNotify() override {
    if (!IsRunning()) {
      return absl::FailedPreconditionError("Not currently notifying");
    }
    should_stop_ = true;
    thread_->join();
    thread_.reset();
    should_stop_ = false;
    return absl::OkStatus();
  }

  bool IsRunning() const override { return thread_ != nullptr; }

 private:
  // Returns max key in alphabetical order from notification
  // Empty string if wait_duration exceeded.
  absl::StatusOr<std::string> WaitForNotification(
      BlobStorageChangeNotifier& change_notifier,
      absl::Duration wait_duration) {
    absl::StatusOr<std::vector<std::string>> changes =
        change_notifier.GetNotifications(
            wait_duration, [this]() { return should_stop_.load(); });
    if (!changes.ok()) {
      return changes.status();
    }
    const auto it = std::max_element((*changes).begin(), (*changes).end());
    return it == (*changes).end() ? "" : *it;
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
    // Don't poll on error.  A backup poll will trigger if necessary.
    if (!notification_key.ok() &&
        !absl::IsDeadlineExceeded(notification_key.status())) {
      return notification_key.status();
    }
    return true;
  }

  void Watch(BlobStorageChangeNotifier& change_notifier,
             BlobStorageClient::DataLocation location, std::string start_after,
             std::function<void(const std::string& key)> callback) {
    LOG(INFO) << "Started to watch " << location;
    std::string last_key = std::move(start_after);
    // Flag starts expired, and forces an initial poll.
    ExpiringFlag expiring_flag;
    uint32_t sequential_failures = 0;
    while (!should_stop_) {
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
        sleep_for_.Duration(backoff_time);
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
      for (const std::string& key : *result) {
        callback(key);
      }
      if (!result->empty()) {
        last_key = result->back();
      } else {
        VLOG(2) << "No new file found";
      }
      expiring_flag.Set(poll_frequency_);
    }
  }

  BlobStorageClient& client_;
  std::unique_ptr<std::thread> thread_;
  std::atomic<bool> should_stop_ = false;
  const absl::Duration poll_frequency_;
  const SleepFor& sleep_for_;
};

}  // namespace

std::unique_ptr<DeltaFileNotifier> DeltaFileNotifier::Create(
    BlobStorageClient& client, const absl::Duration poll_frequency,
    const SleepFor& sleep_for) {
  return std::make_unique<DeltaFileNotifierImpl>(client, poll_frequency,
                                                 sleep_for);
}

}  // namespace kv_server
