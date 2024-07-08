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

#ifndef COMPONENTS_DATA_REALTIME_THREAD_POOL_MANAGER_H_
#define COMPONENTS_DATA_REALTIME_THREAD_POOL_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/data/common/notifier_metadata.h"
#include "components/data/realtime/realtime_notifier.h"
#include "src/telemetry/telemetry.h"

namespace kv_server {

// Manages multiple realtime threads listening to the same queue.
// This allows to increase the throughput. Usually, one of the bottlenecks
// is that the number of batched messages that can be read at the same time
// is limited for each queue. Having multiple threads doing it helps to get
// around it.
class RealtimeThreadPoolManager {
 public:
  virtual ~RealtimeThreadPoolManager() = default;
  // Start realtime notifiers
  virtual absl::Status Start(
      std::function<absl::StatusOr<DataLoadingStats>(const std::string& key)>
          callback) = 0;
  // Stop realtime notifiers
  virtual absl::Status Stop() = 0;
  // Create a realtime thread pool manager that will use the specified
  // numbers of threads to read from the queue.
  static absl::StatusOr<std::unique_ptr<RealtimeThreadPoolManager>> Create(
      NotifierMetadata notifier_metadata, int32_t num_threads,
      // This parameter allows overrides that are used for tests
      std::vector<RealtimeNotifierMetadata> realtime_notifier_metadata = {},
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));
};

}  // namespace kv_server
#endif  // COMPONENTS_DATA_REALTIME_THREAD_POOL_MANAGER_H_
