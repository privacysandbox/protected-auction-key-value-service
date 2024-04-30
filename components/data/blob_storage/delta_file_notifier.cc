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

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "components/data/common/thread_manager.h"
#include "components/errors/retry.h"
#include "public/constants.h"
#include "public/data_loading/filename_utils.h"
#include "src/util/duration.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::ExpiringFlag;
using privacy_sandbox::server_common::SteadyClock;

class DeltaFileNotifierImpl : public DeltaFileNotifier {
 public:
  explicit DeltaFileNotifierImpl(
      BlobStorageClient& client, const absl::Duration poll_frequency,
      std::unique_ptr<SleepFor> sleep_for, SteadyClock& clock,
      BlobPrefixAllowlist blob_prefix_allowlist,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : thread_manager_(ThreadManager::Create("Delta file notifier")),
        client_(client),
        poll_frequency_(poll_frequency),
        sleep_for_(std::move(sleep_for)),
        clock_(clock),
        blob_prefix_allowlist_(std::move(blob_prefix_allowlist)),
        log_context_(log_context) {}

  absl::Status Start(
      BlobStorageChangeNotifier& change_notifier,
      BlobStorageClient::DataLocation location,
      absl::flat_hash_map<std::string, std::string>&& prefix_start_after_map,
      std::function<void(const std::string&)> callback) override {
    return thread_manager_->Start(
        [this, location = std::move(location),
         prefix_start_after_map = std::move(prefix_start_after_map),
         callback = std::move(callback), &change_notifier]() mutable {
          Watch(change_notifier, std::move(location),
                std::move(prefix_start_after_map), std::move(callback));
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
      BlobStorageChangeNotifier& change_notifier, absl::Duration wait_duration);
  // Returns true if we need to check for new blobs.  Returns the error on
  // failure.
  absl::StatusOr<bool> ShouldListBlobs(
      BlobStorageChangeNotifier& change_notifier, ExpiringFlag& expiring_flag,
      const absl::flat_hash_map<std::string, std::string>&
          prefix_start_after_map);
  void Watch(
      BlobStorageChangeNotifier& change_notifier,
      BlobStorageClient::DataLocation location,
      absl::flat_hash_map<std::string, std::string>&& prefix_start_after_map,
      std::function<void(const std::string& key)> callback);

  std::unique_ptr<ThreadManager> thread_manager_;
  BlobStorageClient& client_;
  const absl::Duration poll_frequency_;
  std::unique_ptr<SleepFor> sleep_for_;
  SteadyClock& clock_;
  BlobPrefixAllowlist blob_prefix_allowlist_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

absl::StatusOr<std::string> DeltaFileNotifierImpl::WaitForNotification(
    BlobStorageChangeNotifier& change_notifier, absl::Duration wait_duration) {
  absl::StatusOr<std::vector<std::string>> changes =
      change_notifier.GetNotifications(
          wait_duration, [this]() { return thread_manager_->ShouldStop(); });
  if (!changes.ok()) {
    return changes.status();
  }
  std::string_view max_change = "";
  for (const auto& change : *changes) {
    if (auto blob = ParseBlobName(change);
        change > max_change && IsDeltaFilename(blob.key)) {
      max_change = change;
    }
  }
  return std::string(max_change);
}

// Returns true if we need to check for new blobs.  Returns the error on
// failure.
absl::StatusOr<bool> DeltaFileNotifierImpl::ShouldListBlobs(
    BlobStorageChangeNotifier& change_notifier, ExpiringFlag& expiring_flag,
    const absl::flat_hash_map<std::string, std::string>&
        prefix_start_after_map) {
  if (!expiring_flag.Get()) {
    PS_VLOG(5, log_context_) << "Backup poll";
    return true;
  }
  absl::StatusOr<std::string> notification_key =
      WaitForNotification(change_notifier, expiring_flag.GetTimeRemaining());
  // Don't poll on error.  A backup poll will trigger if necessary.
  if (absl::IsDeadlineExceeded(notification_key.status())) {
    // Deadline exceeded while waiting, trigger backup poll
    PS_VLOG(5, log_context_) << "Backup poll";
    return true;
  }
  if (!notification_key.ok()) {
    return notification_key.status();
  }
  auto notification_blob = ParseBlobName(*notification_key);
  if (auto iter = prefix_start_after_map.find(notification_blob.prefix);
      iter != prefix_start_after_map.end() &&
      notification_blob.key < iter->second) {
    // Ignore notifications for keys we've already seen.
    return false;
  }
  // Return True if there is a Delta file notification
  // False is returned on DeadlineExceeded.
  return blob_prefix_allowlist_.Contains(notification_blob.prefix) &&
         IsDeltaFilename(notification_blob.key);
}

absl::flat_hash_map<std::string, std::vector<std::string>> ListPrefixDeltaFiles(
    BlobStorageClient::DataLocation location,
    const BlobPrefixAllowlist& prefix_allowlist,
    const absl::flat_hash_map<std::string, std::string>& prefix_start_after_map,
    BlobStorageClient& blob_client,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  absl::flat_hash_map<std::string, std::vector<std::string>> prefix_blobs_map;
  for (const auto& blob_prefix : prefix_allowlist.Prefixes()) {
    location.prefix = blob_prefix;
    auto iter = prefix_start_after_map.find(blob_prefix);
    absl::StatusOr<std::vector<std::string>> result = blob_client.ListBlobs(
        location,
        {.prefix = std::string(FilePrefix<FileType::DELTA>()),
         .start_after =
             (iter == prefix_start_after_map.end()) ? "" : iter->second});
    if (!result.ok()) {
      PS_LOG(ERROR, log_context)
          << "Failed to list " << location << ": " << result.status();
      continue;
    }
    if (result->empty()) {
      continue;
    }
    auto& prefix_blobs = prefix_blobs_map[blob_prefix];
    prefix_blobs.reserve(result->size());
    for (const auto& blob : *result) {
      prefix_blobs.push_back(blob);
    }
  }
  return prefix_blobs_map;
}

void DeltaFileNotifierImpl::Watch(
    BlobStorageChangeNotifier& change_notifier,
    BlobStorageClient::DataLocation location,
    absl::flat_hash_map<std::string, std::string>&& prefix_start_after_map,
    std::function<void(const std::string& key)> callback) {
  PS_LOG(INFO, log_context_) << "Started to watch " << location;
  // Flag starts expired, and forces an initial poll.
  ExpiringFlag expiring_flag(clock_);
  uint32_t sequential_failures = 0;
  while (!thread_manager_->ShouldStop()) {
    const absl::StatusOr<bool> should_list_blobs =
        ShouldListBlobs(change_notifier, expiring_flag, prefix_start_after_map);
    if (!should_list_blobs.ok()) {
      ++sequential_failures;
      const absl::Duration backoff_time =
          std::min(expiring_flag.GetTimeRemaining(),
                   ExponentialBackoffForRetry(sequential_failures));
      PS_LOG(ERROR, log_context_)
          << "Failed to get delta file notifications: "
          << should_list_blobs.status() << ".  Waiting for " << backoff_time;
      if (!sleep_for_->Duration(backoff_time)) {
        PS_LOG(ERROR, log_context_)
            << "Failed to sleep for " << backoff_time << ".  SleepFor invalid.";
      }
      continue;
    }
    sequential_failures = 0;
    if (!*should_list_blobs) {
      continue;
    }
    // Set expiring flag before callback for unit test simplicity.
    // Fake clock is moved forward in callback so flag must be set beforehand.
    expiring_flag.Set(poll_frequency_);
    int delta_file_count = 0;
    auto prefix_blobs_map =
        ListPrefixDeltaFiles(location, blob_prefix_allowlist_,
                             prefix_start_after_map, client_, log_context_);
    for (const auto& [prefix, prefix_blobs] : prefix_blobs_map) {
      for (const auto& blob : prefix_blobs) {
        if (!IsDeltaFilename(blob)) {
          continue;
        }
        callback(prefix.empty() ? blob : absl::StrCat(prefix, "/", blob));
        prefix_start_after_map[prefix] = blob;
        delta_file_count++;
      }
    }
    if (delta_file_count == 0) {
      PS_VLOG(2, log_context_) << "No new file found";
    }
  }
}

}  // namespace

std::unique_ptr<DeltaFileNotifier> DeltaFileNotifier::Create(
    BlobStorageClient& client, const absl::Duration poll_frequency,
    BlobPrefixAllowlist blob_prefix_allowlist,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<DeltaFileNotifierImpl>(
      client, poll_frequency, std::make_unique<SleepFor>(),
      SteadyClock::RealClock(), std::move(blob_prefix_allowlist), log_context);
}

// For test only
std::unique_ptr<DeltaFileNotifier> DeltaFileNotifier::Create(
    BlobStorageClient& client, const absl::Duration poll_frequency,
    std::unique_ptr<SleepFor> sleep_for, SteadyClock& clock,
    BlobPrefixAllowlist blob_prefix_allowlist,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<DeltaFileNotifierImpl>(
      client, poll_frequency, std::move(sleep_for), clock,
      std::move(blob_prefix_allowlist), log_context);
}

}  // namespace kv_server
