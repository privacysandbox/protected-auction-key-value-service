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
#include "components/util/sleepfor.h"

namespace kv_server {
namespace {

class GcpBlobStorageChangeNotifier : public BlobStorageChangeNotifier {
 public:
  explicit GcpBlobStorageChangeNotifier(
      std::unique_ptr<ChangeNotifier> notifier,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : notifier_(std::move(notifier)), log_context_(log_context) {}

  ~GcpBlobStorageChangeNotifier() override { sleep_for_.Stop(); }

  absl::StatusOr<std::vector<std::string>> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    // TODO(b/301118821): Implement gcp blob storage change notifier and remove
    // the temporary solution below.
    sleep_for_.Duration(max_wait);
    return absl::DeadlineExceededError(
        "Trigger backup poll before GCP blob storage change notifier is "
        "implemented.");
  }

 private:
  std::unique_ptr<ChangeNotifier> notifier_;
  SleepFor sleep_for_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<BlobStorageChangeNotifier>>
BlobStorageChangeNotifier::Create(
    NotifierMetadata notifier_metadata,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  absl::StatusOr<std::unique_ptr<ChangeNotifier>> notifier =
      ChangeNotifier::Create(std::get<GcpNotifierMetadata>(notifier_metadata),
                             log_context);
  if (!notifier.ok()) {
    return notifier.status();
  }

  return std::make_unique<GcpBlobStorageChangeNotifier>(std::move(*notifier),
                                                        log_context);
}

}  // namespace kv_server
