/*
 * Copyright 2022 Google LLC
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

#ifndef COMPONENTS_DATA_BLOB_STORAGE_BLOB_STORAGE_CHANGE_NOTIFIER_H_
#define COMPONENTS_DATA_BLOB_STORAGE_BLOB_STORAGE_CHANGE_NOTIFIER_H_

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace kv_server {

class BlobStorageChangeNotifier {
 public:
  // Notification publisher details.
  // TODO(b/237669491): Consider putting this into a std::variant rather than
  // allowing both values to be set even though only one will be used.
  struct NotifierMetadata {
    // Used for AWS
    std::string sns_arn;
    // Directory to watch
    std::filesystem::path local_directory;
  };

  virtual ~BlobStorageChangeNotifier() = default;

  // Waits up to `max_wait` to return a vector keys(notifications).
  // `should_stop_callback` is called periodically as a signal to abort prior to
  // `max_wait`.
  virtual absl::StatusOr<std::vector<std::string>> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) = 0;

  static std::unique_ptr<BlobStorageChangeNotifier> Create(
      NotifierMetadata metadata);
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_BLOB_STORAGE_BLOB_STORAGE_CHANGE_NOTIFIER_H_
