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

#ifndef COMPONENTS_DATA_COMMON_THREAD_MANAGER_H_
#define COMPONENTS_DATA_COMMON_THREAD_MANAGER_H_

#include <memory>
#include <string>

#include "components/errors/retry.h"
#include "src/logger/request_context_logger.h"

namespace kv_server {

class ThreadManager {
 public:
  virtual ~ThreadManager() = default;

  // Checks if the ThreadManager is already running.
  // If not, starts a thread on which `watch` is executed.
  // Start and Stop should be called on the same thread as
  // the constructor.
  virtual absl::Status Start(std::function<void()> watch) = 0;

  // Blocks until `IsRunning` is False.
  virtual absl::Status Stop() = 0;

  // Returns False before calling `Start` or after `Stop` is
  // successful.
  virtual bool IsRunning() const = 0;

  virtual bool ShouldStop() = 0;

  static std::unique_ptr<ThreadManager> Create(
      std::string thread_name,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));
};

}  // namespace kv_server
#endif  // COMPONENTS_DATA_COMMON_THREAD_MANAGER_H_
