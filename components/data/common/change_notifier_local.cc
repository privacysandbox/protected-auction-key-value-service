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

#include <filesystem>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "components/data/common/change_notifier.h"
#include "components/util/sleepfor.h"

namespace kv_server {
namespace {

// TODO(b/237669491): This is arbitrary, consider changing it.
constexpr absl::Duration kPollInterval = absl::Seconds(5);

class LocalChangeNotifier : public ChangeNotifier {
 public:
  explicit LocalChangeNotifier(
      std::filesystem::path local_directory,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : local_directory_(local_directory), log_context_(log_context) {
    PS_VLOG(1, log_context_)
        << "Building initial list of local files in directory: "
        << local_directory_.string();
    auto status_or = FindNewFiles({});
    if (!status_or.ok()) {
      PS_LOG(ERROR, log_context_) << "Unable to build initial file list"
                                  << status_or.status().message();
    }
    files_in_directory_ = std::move(status_or.value());
    PS_VLOG(1, log_context_)
        << "Found " << files_in_directory_.size() << " files.";
  }

  ~LocalChangeNotifier() { sleep_for_.Stop(); }

  absl::StatusOr<std::vector<std::string>> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    PS_LOG(INFO, log_context_)
        << "Watching for new files in directory: " << local_directory_.string();

    while (true) {
      if (should_stop_callback()) {
        PS_VLOG(1, log_context_) << "Callback says to stop watching, stopping.";
        return std::vector<std::string>{};
      }

      if (max_wait <= absl::ZeroDuration()) {
        PS_VLOG(1, log_context_)
            << "No new files found within timeout, stopping.";
        return absl::DeadlineExceededError("No messages found");
      }

      auto status_or = FindNewFiles(files_in_directory_);
      if (!status_or.ok()) {
        return status_or.status();
      }
      if (!status_or->empty()) {
        PS_VLOG(1, log_context_) << "Found new local files.";
        // Add the new files to the running list so that they'll be ignored if
        // GetNotifications is called again.
        files_in_directory_.insert(status_or->begin(), status_or->end());
        return std::vector<std::string>{status_or->begin(), status_or->end()};
      }

      max_wait -= kPollInterval;
      sleep_for_.Duration(kPollInterval);
    }
  }

 private:
  // Returns a set of file paths that are in the watched directory but not in
  // previous_files.  Returns just the filename, not the path.
  absl::StatusOr<absl::flat_hash_set<std::string>> FindNewFiles(
      const absl::flat_hash_set<std::string>& previous_files) const {
    std::error_code error_code;
    std::filesystem::directory_iterator it(local_directory_, error_code);
    if (error_code) {
      return absl::InternalError(absl::StrCat(
          "Error creating directory_iterator: ", error_code.message()));
    }

    absl::flat_hash_set<std::string> new_files;
    for (auto const& file : it) {
      const std::string filename =
          std::filesystem::path(file).filename().string();
      if (previous_files.find(filename) == previous_files.end()) {
        PS_VLOG(1, log_context_) << "Found new file: " << filename;
        new_files.emplace(filename);
      }
    }

    return new_files;
  }

  SleepFor sleep_for_;

  std::filesystem::path local_directory_;
  // We can't store std::filesystem::path objects in the set because the paths
  // aren't guaranteed to be canonical so we store the string paths instead.
  absl::flat_hash_set<std::string> files_in_directory_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<ChangeNotifier>> ChangeNotifier::Create(
    NotifierMetadata notifier_metadata,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  std::error_code error_code;
  auto local_notifier_metadata =
      std::get<LocalNotifierMetadata>(notifier_metadata);

  if (!std::filesystem::is_directory(local_notifier_metadata.local_directory,
                                     error_code)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Path is not a directory: ",
                     local_notifier_metadata.local_directory.string()));
  }
  if (error_code) {
    return absl::InvalidArgumentError(
        absl::StrCat("Error checking for directory: ",
                     local_notifier_metadata.local_directory.string(), " ",
                     error_code.message()));
  }

  return std::make_unique<LocalChangeNotifier>(
      std::move(local_notifier_metadata.local_directory), log_context);
}

}  // namespace kv_server
