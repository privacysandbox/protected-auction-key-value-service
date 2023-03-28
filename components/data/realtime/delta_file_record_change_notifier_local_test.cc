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
#include "components/data/common/change_notifier.h"
#include "components/data/common/mocks.h"
#include "components/data/realtime/delta_file_record_change_notifier.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

// We don't need to test the watching of files as that's covered in tests
// for the ChangeNotifier that this class delegates to.
TEST(DeltaFileRecordChangeNotifierTest, SmokeTest) {
  auto mock_change_notifier = std::make_unique<kv_server::MockChangeNotifier>();

  std::unique_ptr<DeltaFileRecordChangeNotifier> notifier =
      DeltaFileRecordChangeNotifier::Create(std::move(mock_change_notifier));
  EXPECT_TRUE(notifier != nullptr);
}

}  // namespace
}  // namespace kv_server
