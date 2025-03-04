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

#include "tools/request_simulation/synthetic_request_generator.h"

#include <string>
#include <vector>

namespace kv_server {

absl::Status SyntheticRequestGenerator::Start() {
  return thread_manager_->Start([this]() { GenerateRequests(); });
}

absl::Status SyntheticRequestGenerator::Stop() {
  return thread_manager_->Stop();
}

bool SyntheticRequestGenerator::IsRunning() const {
  return thread_manager_->IsRunning();
}

void SyntheticRequestGenerator::GenerateRequests() {
  while (!thread_manager_->ShouldStop()) {
    int number_of_requests = requests_fill_qps_ / 2;
    if (rate_limiter_.Acquire(number_of_requests).ok()) {
      std::vector<std::string> messages;
      for (int i = 0; i < number_of_requests; ++i) {
        messages.push_back(request_body_generation_fn_());
      }
      VLOG(7) << "Filled " << number_of_requests;
      message_queue_.Push(std::move(messages));
      VLOG(7) << "Push new message to the queue, current queue size "
              << message_queue_.Size();
    } else {
      VLOG(8) << "Acquire timeout";
    }
    sleep_for_->Duration(absl::Milliseconds(500));
  }
}
}  // namespace kv_server
