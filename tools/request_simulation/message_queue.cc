// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tools/request_simulation/message_queue.h"

#include <utility>
#include <vector>

namespace kv_server {

void MessageQueue::Push(std::string message) {
  absl::MutexLock lock(&mutex_);
  if (queue_.size() < capacity_) {
    queue_.push_back(std::move(message));
  }
}

void MessageQueue::Push(std::vector<std::string> messages) {
  absl::MutexLock lock(&mutex_);
  for (auto& m : messages) {
    if (queue_.size() < capacity_) {
      queue_.push_back(std::move(m));
    }
  }
}

absl::StatusOr<std::string> MessageQueue::Pop() {
  absl::MutexLock lock(&mutex_);
  if (queue_.empty()) {
    return absl::FailedPreconditionError("Queue is empty");
  }
  auto front = queue_.front();
  queue_.pop_front();
  return front;
}

bool MessageQueue::Empty() const {
  absl::MutexLock lock(&mutex_);
  return queue_.empty();
}

size_t MessageQueue::Size() const {
  absl::MutexLock lock(&mutex_);
  return queue_.size();
}

void MessageQueue::Clear() {
  absl::MutexLock lock(&mutex_);
  queue_.clear();
}

}  // namespace kv_server
