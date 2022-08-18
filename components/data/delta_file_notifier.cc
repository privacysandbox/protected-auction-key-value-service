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

#include <thread>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "glog/logging.h"
#include "public/constants.h"

namespace fledge::kv_server {

namespace {

// TODO(b/242344219): Move to flag
constexpr absl::Duration kPollFrequency = absl::Minutes(5);

class DeltaFileNotifierImpl : public DeltaFileNotifier {
 public:
  DeltaFileNotifierImpl(BlobStorageClient& client) : client_(client) {}

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

  void Watch(BlobStorageChangeNotifier& change_notifier,
             BlobStorageClient::DataLocation location, std::string start_after,
             std::function<void(const std::string& key)> callback) {
    LOG(INFO) << "Started to watch " << location;
    std::string last_key = std::move(start_after);
    absl::Time last_poll = absl::InfinitePast();
    while (!should_stop_) {
      // TODO: Switch to a steady clock. b/235082948
      const absl::Time now = absl::Now();
      if (now - last_poll < kPollFrequency) {
        absl::Duration wait_duration = kPollFrequency - (now - last_poll);
        absl::StatusOr<std::string> notification_key =
            WaitForNotification(change_notifier, wait_duration);
        if (notification_key.ok() && *notification_key < last_key) {
          // Ignore notifications for keys we've already seen.
          continue;
        }
        if (!notification_key.ok()) {
          // Don't poll on error.  A backup poll will trigger if necessary.
          // TODO(b/234651372): Certain types of errors should backoff
          // before a retry to avoid a fast loop here.
          if (!absl::IsDeadlineExceeded(notification_key.status())) {
            LOG(ERROR) << "Failed to get delta file notifications: "
                       << notification_key.status();
          }
          continue;
        }
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
      last_poll = absl::Now();
    }
  }

  BlobStorageClient& client_;
  std::unique_ptr<std::thread> thread_;
  std::atomic<bool> should_stop_ = false;
};

}  // namespace

std::unique_ptr<DeltaFileNotifier> DeltaFileNotifier::Create(
    BlobStorageClient& client) {
  return std::make_unique<DeltaFileNotifierImpl>(client);
}

}  // namespace fledge::kv_server
