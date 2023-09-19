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

// TODO(b/296901861): Modify the implementation with GCP specific logic (the
// current implementation is copied from local).

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/data/blob_storage/blob_storage_change_notifier.h"
#include "components/data/common/change_notifier.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::MetricsRecorder;

class GcpBlobStorageChangeNotifier : public BlobStorageChangeNotifier {
 public:
  explicit GcpBlobStorageChangeNotifier(
      std::unique_ptr<ChangeNotifier> notifier, MetricsRecorder& unused)
      : notifier_(std::move(notifier)) {}

  absl::StatusOr<std::vector<std::string>> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    // TODO(b/301118821): Implement gcp blob storage change notifier and remove
    // the temporary solution below.
    return absl::DeadlineExceededError(
        "Trigger backup poll before GCP blob storage change notifier is "
        "implemented.");
  }

 private:
  std::unique_ptr<ChangeNotifier> notifier_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<BlobStorageChangeNotifier>>
BlobStorageChangeNotifier::Create(NotifierMetadata notifier_metadata,
                                  MetricsRecorder& metrics_recorder) {
  absl::StatusOr<std::unique_ptr<ChangeNotifier>> notifier =
      ChangeNotifier::Create(std::get<LocalNotifierMetadata>(notifier_metadata),
                             metrics_recorder);
  if (!notifier.ok()) {
    return notifier.status();
  }

  return std::make_unique<GcpBlobStorageChangeNotifier>(std::move(*notifier),
                                                        metrics_recorder);
}

}  // namespace kv_server
