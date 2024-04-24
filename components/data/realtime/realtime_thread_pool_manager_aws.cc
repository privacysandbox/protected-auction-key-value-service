/*
 * Copyright 2023 Google LLC
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

#include <memory>
#include <string>

#include "components/data/realtime/realtime_thread_pool_manager.h"

namespace kv_server {
namespace {

class RealtimeThreadPoolManagerAws : public RealtimeThreadPoolManager {
 public:
  explicit RealtimeThreadPoolManagerAws(
      std::vector<std::unique_ptr<RealtimeNotifier>> realtime_notifiers,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : realtime_notifiers_(std::move(realtime_notifiers)),
        log_context_(log_context) {}

  ~RealtimeThreadPoolManagerAws() override { Stop(); }

  absl::Status Start(
      std::function<absl::StatusOr<DataLoadingStats>(const std::string& key)>
          callback) override {
    for (auto& realtime_notifier : realtime_notifiers_) {
      if (realtime_notifier == nullptr) {
        std::string error_message =
            "Realtime realtime_notifier is nullptr, realtime data "
            "loading disabled.";
        PS_LOG(ERROR, log_context_) << error_message;
        return absl::InvalidArgumentError(std::move(error_message));
      }
      auto status = realtime_notifier->Start(callback);
      if (!status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  absl::Status Stop() override {
    absl::Status status = absl::OkStatus();
    for (auto& realtime_notifier : realtime_notifiers_) {
      if (realtime_notifier == nullptr) {
        PS_LOG(ERROR, log_context_) << "Realtime realtime_notifier is nullptr";
        continue;
      }
      if (realtime_notifier->IsRunning()) {
        auto current_status = realtime_notifier->Stop();
        status.Update(current_status);
        if (!current_status.ok()) {
          PS_LOG(ERROR, log_context_) << current_status.message();
          // we still want to try to stop others
          continue;
        }
      }
    }
    return status;
  }

 private:
  std::vector<std::unique_ptr<RealtimeNotifier>> realtime_notifiers_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};
}  // namespace

absl::StatusOr<std::unique_ptr<RealtimeThreadPoolManager>>
RealtimeThreadPoolManager::Create(
    NotifierMetadata notifier_metadata, int32_t num_threads,
    std::vector<RealtimeNotifierMetadata> realtime_notifier_metadata,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  std::vector<std::unique_ptr<RealtimeNotifier>> realtime_notifier;
  for (int i = 0; i < num_threads; i++) {
    RealtimeNotifierMetadata realtime_notifier_metadatum =
        realtime_notifier_metadata.size() < num_threads
            ? RealtimeNotifierMetadata{}
            : std::move(realtime_notifier_metadata[i]);
    auto maybe_realtime_notifier = RealtimeNotifier::Create(
        notifier_metadata, std::move(realtime_notifier_metadatum), log_context);
    if (!maybe_realtime_notifier.ok()) {
      return maybe_realtime_notifier.status();
    }
    realtime_notifier.push_back(std::move(*maybe_realtime_notifier));
  }
  return std::make_unique<RealtimeThreadPoolManagerAws>(
      std::move(realtime_notifier), log_context);
}

}  // namespace kv_server
