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

#ifndef COMPONENTS_DATA_BLOB_STORAGE_DELTA_FILE_NOTIFIER_H_
#define COMPONENTS_DATA_BLOB_STORAGE_DELTA_FILE_NOTIFIER_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "components/data/blob_storage/blob_prefix_allowlist.h"
#include "components/data/blob_storage/blob_storage_change_notifier.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/common/thread_manager.h"
#include "components/errors/retry.h"
#include "components/util/sleepfor.h"
#include "src/util/duration.h"

namespace kv_server {

class DeltaFileNotifier {
 public:
  virtual ~DeltaFileNotifier() = default;

  // Starts to monitor new Delta files in `location` whose names are after
  // `start_after`.
  // `BlobStorageChangeNotifier` must not be deallocated until `Stop`
  // returns or `DeltaFileNotifier` is destroyed.
  // Calls `callback` on every Delta file found, in
  // ascending order of the file name.
  // `callback` blocks this object's operations so it should
  // return as soon as possible.
  //
  // Start and Stop should be called on the same thread as
  // the constructor.
  virtual absl::Status Start(
      BlobStorageChangeNotifier& change_notifier,
      BlobStorageClient::DataLocation location,
      absl::flat_hash_map<std::string, std::string>&& prefix_start_after_map,
      std::function<void(const std::string&)> callback) = 0;

  // Blocks until `IsRunning` is False.
  virtual absl::Status Stop() = 0;

  // Returns False before calling `Start` or after `Stop` is
  // successful.
  virtual bool IsRunning() const = 0;

  static std::unique_ptr<DeltaFileNotifier> Create(
      BlobStorageClient& client,
      const absl::Duration poll_frequency = absl::Minutes(5),
      BlobPrefixAllowlist blob_prefix_allowlist = BlobPrefixAllowlist(""),
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));

  // Used for test
  static std::unique_ptr<DeltaFileNotifier> Create(
      BlobStorageClient& client, const absl::Duration poll_frequency,
      std::unique_ptr<SleepFor> sleep_for,
      privacy_sandbox::server_common::SteadyClock& clock,
      BlobPrefixAllowlist blob_prefix_allowlist = BlobPrefixAllowlist(""),
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));
};

}  // namespace kv_server
#endif  // COMPONENTS_DATA_BLOB_STORAGE_DELTA_FILE_NOTIFIER_H_
