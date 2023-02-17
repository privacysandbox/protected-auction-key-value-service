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

namespace kv_server {
namespace {

TEST(LocalBlobStorageClientTest, NotImplemented) {
  BlobStorageChangeNotifier::NotifierMetadata metadata;
  std::unique_ptr<BlobStorageChangeNotifier> notifier =
      BlobStorageChangeNotifier::Create(metadata);
  ASSERT_TRUE(notifier != nullptr);

  absl::Duration duration;
  auto should_stop_callback = []() { return false; };

  EXPECT_EQ(absl::StatusCode::kUnimplemented,
            notifier->GetNotifications(duration, should_stop_callback)
                .status()
                .code());
}

// TODO(237669491): Add tests here

}  // namespace
}  // namespace kv_server
