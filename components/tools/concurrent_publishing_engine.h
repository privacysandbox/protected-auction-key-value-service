// Copyright 2023 Google LLC
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

#ifndef COMPONENTS_TOOLS_CONCURRENT_PUBLISHING_ENGINE_H_
#define COMPONENTS_TOOLS_CONCURRENT_PUBLISHING_ENGINE_H_

#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "absl/log/log.h"
#include "components/tools/publisher_service.h"
#include "src/util/duration.h"

namespace kv_server {

struct RealtimeMessage {
  std::string message;
  std::optional<int> shard_num;
};

// ConcurrentPublishingEngine concurrently reads message of the queue and
// publishes them to the pubsub/sns specified by `notifier_metadata`.
class ConcurrentPublishingEngine {
 public:
  // Create ConcurrentPublishingEngine.
  // `notifier_metadata` specifies the pubsub/sns where the engine will be
  // publishing to.
  // `queue_mutex` and `updates_queue` are not owned by
  // ConcurrentPublishingEngine and must outlive it.
  ConcurrentPublishingEngine(int insertion_num_threads,
                             NotifierMetadata notifier_metadata,
                             int files_insertion_rate, absl::Mutex& queue_mutex,
                             std::queue<RealtimeMessage>& updates_queue);
  // Starts the publishing engine.
  void Start();
  // Stops the publishing engine.
  void Stop();

  // ConcurrentPublishingEngine is neither copyable nor movable.
  ConcurrentPublishingEngine(const ConcurrentPublishingEngine&) = delete;
  ConcurrentPublishingEngine& operator=(const ConcurrentPublishingEngine&) =
      delete;

 private:
  std::optional<RealtimeMessage> Pop(absl::Condition& has_new_event);
  bool ShouldStop();
  void ConsumeAndPublish(int thread_idx);

  const int insertion_num_threads_;
  const NotifierMetadata notifier_metadata_;
  const int files_insertion_rate_;
  absl::Mutex& mutex_;
  bool stop_ ABSL_GUARDED_BY(mutex_) = false;
  bool HasNewMessageToProcess() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  std::queue<RealtimeMessage>& updates_queue_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::unique_ptr<std::thread>> publishers_;
  privacy_sandbox::server_common::SteadyClock& clock_ =
      privacy_sandbox::server_common::SteadyClock::RealClock();
};

}  // namespace kv_server

#endif  // COMPONENTS_TOOLS_CONCURRENT_PUBLISHING_ENGINE_H_
