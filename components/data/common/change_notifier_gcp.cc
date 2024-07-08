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

// TODO(b/296901861): Modify the implementation with GCP specific logic (the
// current implementation is copied from local).

#include <filesystem>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/clock.h"
#include "components/data/common/change_notifier.h"
#include "components/util/sleepfor.h"

namespace kv_server {
namespace {

// TODO fix this or delete

class GcpChangeNotifier : public ChangeNotifier {
 public:
  explicit GcpChangeNotifier(
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : log_context_(log_context) {}
  ~GcpChangeNotifier() { sleep_for_.Stop(); }

  absl::StatusOr<std::vector<std::string>> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    std::vector<std::string> result;
    return result;
  }

 private:
  SleepFor sleep_for_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<ChangeNotifier>> ChangeNotifier::Create(
    NotifierMetadata notifier_metadata,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<GcpChangeNotifier>(log_context);
}

}  // namespace kv_server
