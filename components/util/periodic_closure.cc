// Copyright 2022 Google LLC
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

#include "components/util/periodic_closure.h"

#include <thread>
#include <utility>

#include "absl/synchronization/notification.h"

namespace fledge::kv_server {
namespace {

class PeriodicClosureImpl : public PeriodicClosure {
 public:
  ~PeriodicClosureImpl() { Stop(); }

  absl::Status StartNow(absl::Duration interval,
                        std::function<void()> closure) override {
    return StartInternal(interval, /*run_first=*/true, std::move(closure));
  }

  absl::Status StartDelayed(absl::Duration interval,
                            std::function<void()> closure) override {
    return StartInternal(interval, /*run_first=*/false, std::move(closure));
  }

  void Stop() override {
    if (thread_ && !notification_.HasBeenNotified()) {
      notification_.Notify();
      thread_->join();
    }
  }

  bool IsRunning() const override { return thread_ && thread_->joinable(); }

 private:
  absl::Status StartInternal(absl::Duration interval, bool run_first,
                             std::function<void()> closure) {
    if (IsRunning()) {
      return absl::FailedPreconditionError("Already running.");
    }
    if (notification_.HasBeenNotified()) {
      return absl::FailedPreconditionError("Already ran.");
    }
    thread_ = std::make_unique<std::thread>(
        [this, interval, run_first, closure = std::move(closure)] {
          if (run_first) {
            closure();
          }
          while (!notification_.WaitForNotificationWithTimeout(interval)) {
            closure();
          }
        });
    return absl::OkStatus();
  }

  std::unique_ptr<std::thread> thread_;
  absl::Notification notification_;
};
}  // namespace
std::unique_ptr<PeriodicClosure> PeriodicClosure::Create() {
  return std::make_unique<PeriodicClosureImpl>();
}
}  // namespace fledge::kv_server
