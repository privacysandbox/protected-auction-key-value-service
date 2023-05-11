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
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "components/data/blob_storage/blob_storage_change_notifier.h"
#include "gtest/gtest.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::MockMetricsRecorder;

// We don't need to test the watching of files as that's covered in tests
// for the ChangeNotifier that this class delegates to.
TEST(BlobStorageChangeNotifierLocalTest, SmokeTest) {
  LocalNotifierMetadata metadata{
      .local_directory = std::filesystem::path(::testing::TempDir())};

  MockMetricsRecorder metrics_recorder;
  absl::StatusOr<std::unique_ptr<BlobStorageChangeNotifier>> notifier =
      BlobStorageChangeNotifier::Create(metadata, metrics_recorder);
  EXPECT_TRUE(notifier.ok());
}

}  // namespace
}  // namespace kv_server
