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

#include "components/data/realtime/realtime_notifier_metadata.h"
#include "components/data/realtime/realtime_thread_pool_manager.h"

namespace kv_server {
namespace {

class RealtimeThreadPoolManagerGCP : public RealtimeThreadPoolManager {
 public:
  explicit RealtimeThreadPoolManagerGCP(
      std::unique_ptr<RealtimeNotifier> realtime_notifier)
      : realtime_notifier_(std::move(realtime_notifier)) {}
  ~RealtimeThreadPoolManagerGCP() override { Stop(); }

  absl::Status Start(
      std::function<absl::StatusOr<DataLoadingStats>(const std::string& key)>
          callback) override {
    return realtime_notifier_->Start(std::move(callback));
  }

  absl::Status Stop() override {
    if (realtime_notifier_->IsRunning()) {
      auto status = realtime_notifier_->Stop();
      if (!status.ok()) {
        LOG(ERROR) << status.message();
      }
      return status;
    }
    return absl::OkStatus();
  }

 private:
  std::unique_ptr<RealtimeNotifier> realtime_notifier_;
};
}  // namespace

absl::StatusOr<std::unique_ptr<RealtimeThreadPoolManager>>
RealtimeThreadPoolManager::Create(
    privacy_sandbox::server_common::MetricsRecorder& metrics_recorder,
    NotifierMetadata notifier_metadata, int32_t num_threads,
    std::vector<RealtimeNotifierMetadata> realtime_notifier_metadata) {
  RealtimeNotifierMetadata realtime_notifier_metadatum =
      realtime_notifier_metadata.empty()
          ? RealtimeNotifierMetadata{}
          : std::move(realtime_notifier_metadata[0]);
  auto maybe_realtime_notifier =
      RealtimeNotifier::Create(metrics_recorder, std::move(notifier_metadata),
                               std::move(realtime_notifier_metadatum));
  if (!maybe_realtime_notifier.ok()) {
    return maybe_realtime_notifier.status();
  }
  return std::make_unique<RealtimeThreadPoolManagerGCP>(
      std::move(*maybe_realtime_notifier));
}

}  // namespace kv_server
