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

#ifndef TOOLS_REQUEST_SIMULATION_SYNTHETIC_REQUEST_GENERATOR_H_
#define TOOLS_REQUEST_SIMULATION_SYNTHETIC_REQUEST_GENERATOR_H_

#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "components/data/common/thread_manager.h"
#include "tools/request_simulation/message_queue.h"
#include "tools/request_simulation/rate_limiter.h"

namespace kv_server {

struct SyntheticRequestGenOption {
  int number_of_keys_per_request;
  int key_size_in_bytes;
};

// Generates synthetic requests in a single thread
// at rate controlled by rate limiter passed in the
// constructor
class SyntheticRequestGenerator {
 public:
  SyntheticRequestGenerator(
      MessageQueue& message_queue, RateLimiter& rate_limiter,
      std::unique_ptr<privacy_sandbox::server_common::SleepFor> sleep_for,
      int64_t requests_fill_qps,
      absl::AnyInvocable<std::string()> request_body_generation_fn)
      : thread_manager_(ThreadManager::Create("Synthetic request generator")),
        message_queue_(message_queue),
        rate_limiter_(rate_limiter),
        sleep_for_(std::move(sleep_for)),
        requests_fill_qps_(requests_fill_qps),
        request_body_generation_fn_(std::move(request_body_generation_fn)) {}
  // Starts the thread of generating requests
  absl::Status Start();
  // Stops the thread of generating requests
  absl::Status Stop();
  // Check if the thread of generating requests is running
  bool IsRunning() const;
  virtual ~SyntheticRequestGenerator() = default;
  // SyntheticRequestGenerator is neither copyable nor movable.
  SyntheticRequestGenerator(const SyntheticRequestGenerator&) = delete;
  SyntheticRequestGenerator& operator=(const SyntheticRequestGenerator&) =
      delete;

 private:
  // The actual function that generates requests
  void GenerateRequests();
  std::unique_ptr<ThreadManager> thread_manager_;
  kv_server::MessageQueue& message_queue_;
  RateLimiter& rate_limiter_;
  std::unique_ptr<privacy_sandbox::server_common::SleepFor> sleep_for_;
  int64_t requests_fill_qps_;
  absl::AnyInvocable<std::string()> request_body_generation_fn_;
};
}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_SYNTHETIC_REQUEST_GENERATOR_H_
