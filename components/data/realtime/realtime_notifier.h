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

#ifndef COMPONENTS_DATA_REALTIME_NOTIFIER_H_
#define COMPONENTS_DATA_REALTIME_NOTIFIER_H_

#include <memory>
#include <string>

#include "components/data/common/notifier_metadata.h"
#include "components/data/common/thread_manager.h"
#include "components/data/realtime/realtime_notifier_metadata.h"
#include "components/errors/retry.h"
#include "components/util/sleepfor.h"
#include "src/logger/request_context_logger.h"

namespace kv_server {
struct DataLoadingStats {
  int64_t total_updated_records = 0;
  int64_t total_deleted_records = 0;
  int64_t total_dropped_records = 0;
};

class RealtimeNotifier {
 public:
  virtual ~RealtimeNotifier() = default;

  // Starts to monitor high priority updates.
  // Calls `callback` on every high priority update.
  // `callback` blocks this object's operations so it should
  // return as soon as possible.
  // Start and Stop should be called on the same thread as
  // the constructor.
  virtual absl::Status Start(
      std::function<absl::StatusOr<DataLoadingStats>(const std::string& key)>
          callback) = 0;

  // Blocks until `IsRunning` is False.
  virtual absl::Status Stop() = 0;

  // Returns False before calling `Start` or after `Stop` is
  // successful.
  virtual bool IsRunning() const = 0;

  // Creates RealtimeNotifier.
  static absl::StatusOr<std::unique_ptr<RealtimeNotifier>> Create(
      NotifierMetadata notifier_metadata,
      // This parameter allows overrides that are used for tests
      RealtimeNotifierMetadata realtime_notifier_metadata = {},
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));
};

}  // namespace kv_server
#endif  // COMPONENTS_DATA_REALTIME_NOTIFIER_H_
