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

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "components/data/common/change_notifier.h"
#include "glog/logging.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

class ChangeNotifierLocalTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::error_code error_code;
    std::filesystem::create_directories(::testing::TempDir(), error_code);
    if (error_code) {
      LOG(WARNING) << error_code.message();
    }
  }

  void TearDown() override {
    std::error_code error_code;
    std::filesystem::remove_all(::testing::TempDir(), error_code);
    if (error_code) {
      LOG(WARNING) << error_code.message();
    }
  }

  std::filesystem::path CreateFileInTmpDir(const std::string& filename) {
    const std::filesystem::path path =
        std::filesystem::path(::testing::TempDir()) / filename;
    std::ofstream file(path);
    file << "arbitrary file contents";
    return path;
  }
};

TEST_F(ChangeNotifierLocalTest, NotDirectory) {
  std::filesystem::path file = CreateFileInTmpDir("filename");

  absl::StatusOr<std::unique_ptr<ChangeNotifier>> notifier =
      ChangeNotifier::Create(kv_server::LocalNotifierMetadata{file.string()});
  EXPECT_EQ(notifier.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(ChangeNotifierLocalTest, CallbackAbortsWatchImmediately) {
  absl::StatusOr<std::unique_ptr<ChangeNotifier>> notifier =
      ChangeNotifier::Create(
          kv_server::LocalNotifierMetadata{::testing::TempDir()});
  ASSERT_TRUE(notifier.ok());

  // There is a new file here but the callback says to abort immediately so it's
  // not seen.
  CreateFileInTmpDir("filename");
  auto should_stop_callback = []() { return true; };

  const auto status_or =
      (*notifier)->GetNotifications(absl::Seconds(1), should_stop_callback);
  ASSERT_TRUE(status_or.ok());
  EXPECT_EQ(*status_or, std::vector<std::string>{});
}

TEST_F(ChangeNotifierLocalTest, ZeroDurationAbortsWatchImmediately) {
  absl::StatusOr<std::unique_ptr<ChangeNotifier>> notifier =
      ChangeNotifier::Create(
          kv_server::LocalNotifierMetadata{::testing::TempDir()});
  ASSERT_TRUE(notifier.ok());

  // There is a new file here but the max time to wait is zero seconds so the
  // notifier immediately aborts.
  CreateFileInTmpDir("filename");
  auto should_stop_callback = []() { return false; };

  const auto status_or =
      (*notifier)->GetNotifications(absl::ZeroDuration(), should_stop_callback);
  EXPECT_EQ(absl::StatusCode::kDeadlineExceeded, status_or.status().code());
}

TEST_F(ChangeNotifierLocalTest, NoChangesInEmptyDirectory) {
  absl::StatusOr<std::unique_ptr<ChangeNotifier>> notifier =
      ChangeNotifier::Create(
          kv_server::LocalNotifierMetadata{::testing::TempDir()});
  ASSERT_TRUE(notifier.ok());

  auto should_stop_callback = []() { return false; };

  const auto status_or =
      (*notifier)->GetNotifications(absl::Seconds(1), should_stop_callback);
  EXPECT_EQ(absl::StatusCode::kDeadlineExceeded, status_or.status().code());
}

TEST_F(ChangeNotifierLocalTest, NoChangesInNonEmptyDirectory) {
  // This file is created before the Notifier is so it won't be seen as a
  // change when we call GetNotifications().
  std::filesystem::path filepath = CreateFileInTmpDir("filename");

  absl::StatusOr<std::unique_ptr<ChangeNotifier>> notifier =
      ChangeNotifier::Create(
          kv_server::LocalNotifierMetadata{::testing::TempDir()});
  ASSERT_TRUE(notifier.ok());
  auto should_stop_callback = []() { return false; };

  const auto status_or =
      (*notifier)->GetNotifications(absl::Seconds(1), should_stop_callback);
  EXPECT_EQ(absl::StatusCode::kDeadlineExceeded, status_or.status().code());
}

TEST_F(ChangeNotifierLocalTest, DirectoryHasChanges) {
  absl::StatusOr<std::unique_ptr<ChangeNotifier>> notifier =
      ChangeNotifier::Create(
          kv_server::LocalNotifierMetadata{::testing::TempDir()});
  ASSERT_TRUE(notifier.ok());

  // This call happens after the Notifier is created so it should be picked up.
  std::filesystem::path filepath = CreateFileInTmpDir("filename");

  auto should_stop_callback = []() { return false; };
  const auto status_or =
      (*notifier)->GetNotifications(absl::Seconds(1), should_stop_callback);

  ASSERT_TRUE(status_or.ok());
  EXPECT_EQ(status_or.value(),
            std::vector<std::string>{filepath.filename().string()});
}

TEST_F(ChangeNotifierLocalTest, MultipleGetNotificationsCalls) {
  absl::StatusOr<std::unique_ptr<ChangeNotifier>> notifier =
      ChangeNotifier::Create(
          kv_server::LocalNotifierMetadata{::testing::TempDir()});
  ASSERT_TRUE(notifier.ok());
  auto should_stop_callback = []() { return false; };

  // The first time we call GetNotifications there is one new file.
  {
    std::filesystem::path filepath_one = CreateFileInTmpDir("one");
    const auto status_or =
        (*notifier)->GetNotifications(absl::Seconds(1), should_stop_callback);
    ASSERT_TRUE(status_or.ok());
    EXPECT_EQ(status_or.value(),
              std::vector<std::string>{filepath_one.filename().string()});
  }

  // The second time there's also one file but it's not the same one.
  {
    std::filesystem::path filepath_two = CreateFileInTmpDir("two");
    const auto status_or =
        (*notifier)->GetNotifications(absl::Seconds(1), should_stop_callback);
    ASSERT_TRUE(status_or.ok());
    EXPECT_EQ(status_or.value(),
              std::vector<std::string>{filepath_two.filename().string()});
  }
}

}  // namespace
}  // namespace kv_server
