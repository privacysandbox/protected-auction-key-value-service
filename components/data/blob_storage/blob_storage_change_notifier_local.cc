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

#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/data/blob_storage/blob_storage_change_notifier.h"

namespace kv_server {
namespace {

class LocalBlobStorageChangeNotifier : public BlobStorageChangeNotifier {
 public:
  explicit LocalBlobStorageChangeNotifier(
      std::filesystem::path local_directory) {}

  absl::StatusOr<std::vector<std::string>> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    return absl::UnimplementedError("TODO(b/237669491)");
  }
};

}  // namespace

std::unique_ptr<BlobStorageChangeNotifier> BlobStorageChangeNotifier::Create(
    NotifierMetadata metadata) {
  return std::make_unique<LocalBlobStorageChangeNotifier>(
      std::move(metadata.local_directory));
}

}  // namespace kv_server
