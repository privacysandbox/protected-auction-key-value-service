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

#ifndef COMPONENTS_DATA_DELTA_FILE_NOTIFIER_H_
#define COMPONENTS_DATA_DELTA_FILE_NOTIFIER_H_

#include "components/data/blob_storage_change_notifier.h"
#include "components/data/blob_storage_client.h"

namespace fledge::kv_server {

class DeltaFileNotifier {
 public:
  virtual ~DeltaFileNotifier() = default;

  // Starts to monitor new Delta files in `location` whose names are after
  // `start_after`.
  // `BlobStorageChangeNotifier` must not be deallocated until `StopNotify`
  // returns or `DeltaFileNotifier` is destroyed.
  // Calls `callback` on every Delta file found, in
  // ascending order of the file name.
  // `callback` blocks this object's operations so it should
  // return as soon as possible.
  //
  // StartNotify and StopNotify should be called on the same thread as
  // the constructor.
  virtual absl::Status StartNotify(
      BlobStorageChangeNotifier& change_notifier,
      BlobStorageClient::DataLocation location, std::string start_after,
      std::function<void(const std::string& key)> callback) = 0;

  // Blocks until `IsRunning` is False.
  virtual absl::Status StopNotify() = 0;

  // Returns False before calling `StartNotify` or after `StopNotify` is
  // successful.
  virtual bool IsRunning() const = 0;

  static std::unique_ptr<DeltaFileNotifier> Create(BlobStorageClient& client);
};

}  // namespace fledge::kv_server
#endif  // COMPONENTS_DATA_DELTA_FILE_NOTIFIER_H_
