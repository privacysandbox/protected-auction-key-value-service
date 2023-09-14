/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TOOLS_REQUEST_SIMULATION_MESSAGE_QUEUE_H_
#define TOOLS_REQUEST_SIMULATION_MESSAGE_QUEUE_H_

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"

namespace kv_server {

// Synchronized message queue to stage the request body
class MessageQueue {
 public:
  explicit MessageQueue(int64_t capacity) : capacity_(capacity) {}
  // Pushes new message to the queue
  void Push(std::string message);
  // Pushes new messages to the queue
  void Push(std::vector<std::string> messages);
  // Pops off message from the queue
  absl::StatusOr<std::string> Pop();
  // Checks if the queue is empty
  bool Empty() const;
  // Returns the size of the queue
  size_t Size() const;
  // Clears the queue
  void Clear();
  ~MessageQueue() = default;

  // MessageQueue is neither copyable nor movable.
  MessageQueue(const MessageQueue&) = delete;
  MessageQueue& operator=(const MessageQueue&) = delete;

 private:
  mutable absl::Mutex mutex_;
  int64_t capacity_ ABSL_GUARDED_BY(mutex_);
  std::deque<std::string> queue_ ABSL_GUARDED_BY(mutex_);
};
}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_MESSAGE_QUEUE_H_
