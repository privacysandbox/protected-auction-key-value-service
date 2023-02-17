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

#include "components/data/common/thread_notifier.h"

#include <algorithm>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "components/errors/retry.h"
#include "components/util/duration.h"
#include "glog/logging.h"
#include "public/constants.h"
#include "public/data_loading/filename_utils.h"

namespace kv_server {
namespace {
class ThreadNotifierImpl : public ThreadNotifier {
 public:
  explicit ThreadNotifierImpl(std::string notifier_name)
      : notifier_name_(std::move(notifier_name)) {}

  ~ThreadNotifierImpl() {
    if (!IsRunning()) return;
    if (const auto s = Stop(); !s.ok()) {
      LOG(ERROR) << notifier_name_ << " failed to stop notifier: " << s;
    }
  }

  absl::Status Start(std::function<void()> watch) override {
    if (IsRunning()) {
      return absl::FailedPreconditionError("Already notifying");
    }
    LOG(INFO) << notifier_name_ << "Creating thread for watching files";
    thread_ = std::make_unique<std::thread>(watch);
    return absl::OkStatus();
  }

  absl::Status Stop() override {
    if (!IsRunning()) {
      return absl::FailedPreconditionError("Not currently notifying");
    }
    should_stop_ = true;
    thread_->join();
    thread_.reset();
    should_stop_ = false;
    return absl::OkStatus();
  }

  bool IsRunning() const override { return thread_ != nullptr; }

  bool ShouldStop() override { return should_stop_.load(); }

 private:
  std::unique_ptr<std::thread> thread_;
  std::atomic<bool> should_stop_ = false;
  std::string notifier_name_;
};

}  // namespace

std::unique_ptr<ThreadNotifier> ThreadNotifier::Create(
    std::string notifier_name) {
  return std::make_unique<ThreadNotifierImpl>(std::move(notifier_name));
}

}  // namespace kv_server
