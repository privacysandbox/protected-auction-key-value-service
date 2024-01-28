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

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "components/data/common/change_notifier.h"
#include "gtest/gtest.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

class ChangeNotifierGcpTest : public ::testing::Test {
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

TEST_F(ChangeNotifierGcpTest, CallbackAbortsWatchImmediately) {
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

}  // namespace
}  // namespace kv_server
