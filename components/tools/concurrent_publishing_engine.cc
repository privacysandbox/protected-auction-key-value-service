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

#include "components/tools/concurrent_publishing_engine.h"

#include <utility>

namespace kv_server {

ConcurrentPublishingEngine::ConcurrentPublishingEngine(
    int insertion_num_threads, NotifierMetadata notifier_metadata,
    int files_insertion_rate, absl::Mutex& queue_mutex,
    std::queue<RealtimeMessage>& updates_queue)
    : insertion_num_threads_(insertion_num_threads),
      notifier_metadata_(std::move(notifier_metadata)),
      files_insertion_rate_(files_insertion_rate),
      mutex_(queue_mutex),
      updates_queue_(updates_queue) {}

void ConcurrentPublishingEngine::Start() {
  publishers_.reserve(insertion_num_threads_);
  for (int i = 0; i < insertion_num_threads_; i++) {
    publishers_.emplace_back(
        std::make_unique<std::thread>([i, this] { ConsumeAndPublish(i); }));
  }
}

void ConcurrentPublishingEngine::Stop() {
  {
    absl::MutexLock l(&mutex_);
    stop_ = true;
  }
  for (auto& publisher_thread : publishers_) {
    publisher_thread->join();
  }
}

bool ConcurrentPublishingEngine::HasNewMessageToProcess() const {
  return !updates_queue_.empty() || stop_;
}

std::optional<RealtimeMessage> ConcurrentPublishingEngine::Pop(
    absl::Condition& has_new_event) {
  absl::MutexLock lock(&mutex_, has_new_event);
  if (stop_) {
    LOG(INFO) << "Thread for new file processing stopped";
    return std::nullopt;
  }
  auto file_encoded = updates_queue_.front();
  updates_queue_.pop();
  return file_encoded;
}

bool ConcurrentPublishingEngine::ShouldStop() {
  absl::MutexLock l(&mutex_);
  return stop_;
}

void ConcurrentPublishingEngine::ConsumeAndPublish(int thread_idx) {
  const auto start = clock_.Now();
  int delta_file_index = 1;
  auto batch_end = clock_.Now() + absl::Seconds(1);
  auto maybe_msg_service = PublisherService::Create(notifier_metadata_);
  if (!maybe_msg_service.ok()) {
    LOG(ERROR) << "Failed creating a publisher service";
    return;
  }
  auto msg_service = std::move(*maybe_msg_service);
  absl::Condition has_new_event(
      this, &ConcurrentPublishingEngine::HasNewMessageToProcess);

  while (!ShouldStop()) {
    auto message = Pop(has_new_event);
    if (!message.has_value()) {
      return;
    }
    LOG(INFO) << ": Inserting to the SNS: " << delta_file_index
              << " Thread idx " << thread_idx;
    auto status = msg_service->Publish(message->message, message->shard_num);
    if (!status.ok()) {
      LOG(ERROR) << status;
    }
    delta_file_index++;
    // rate limit to N files per second
    if (delta_file_index % files_insertion_rate_ == 0) {
      if (batch_end > clock_.Now()) {
        absl::SleepFor(batch_end - clock_.Now());
        LOG(INFO) << ": sleeping " << delta_file_index;
      }
      batch_end += absl::Seconds(1);
    }
  }

  int64_t elapsed_seconds = absl::ToInt64Seconds(clock_.Now() - start);
  LOG(INFO) << "Total inserted: " << delta_file_index << " Seconds elapsed "
            << elapsed_seconds << " Thread idx " << thread_idx;
  if (elapsed_seconds > 0) {
    LOG(INFO) << "Actual rate " << (delta_file_index / elapsed_seconds);
  }
}

}  // namespace kv_server
