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

#include "components/data/common/thread_manager.h"

#include <algorithm>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "components/errors/retry.h"
#include "public/constants.h"
#include "public/data_loading/filename_utils.h"
#include "src/util/duration.h"

namespace kv_server {
namespace {
class ThreadManagerImpl : public ThreadManager {
 public:
  explicit ThreadManagerImpl(
      std::string thread_name,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : thread_name_(std::move(thread_name)), log_context_(log_context) {}

  ~ThreadManagerImpl() {
    if (!IsRunning()) return;
    PS_VLOG(8, log_context_)
        << thread_name_ << " In destructor. Attempting to stop the thread.";
    if (const auto s = Stop(); !s.ok()) {
      PS_LOG(ERROR, log_context_) << thread_name_ << " failed to stop: " << s;
    }
  }

  absl::Status Start(std::function<void()> watch) override {
    if (IsRunning()) {
      return absl::FailedPreconditionError("Already running");
    }
    PS_LOG(INFO, log_context_)
        << thread_name_ << " Creating thread for processing";
    thread_ = std::make_unique<std::thread>(watch);
    return absl::OkStatus();
  }

  absl::Status Stop() override {
    PS_VLOG(8, log_context_) << thread_name_ << "Stop called";
    if (!IsRunning()) {
      PS_LOG(ERROR, log_context_) << thread_name_ << " not running";
      return absl::FailedPreconditionError("Not currently running");
    }
    should_stop_ = true;
    thread_->join();
    PS_VLOG(8, log_context_) << thread_name_ << " joined";
    thread_.reset();
    should_stop_ = false;
    return absl::OkStatus();
  }

  bool IsRunning() const override { return thread_ != nullptr; }

  bool ShouldStop() override { return should_stop_.load(); }

 private:
  std::unique_ptr<std::thread> thread_;
  std::atomic<bool> should_stop_ = false;
  std::string thread_name_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

}  // namespace

std::unique_ptr<ThreadManager> ThreadManager::Create(
    std::string thread_name,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<ThreadManagerImpl>(std::move(thread_name),
                                             log_context);
}

}  // namespace kv_server
