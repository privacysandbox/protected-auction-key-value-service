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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/data/common/mocks.h"
#include "components/data/common/mocks_aws.h"
#include "components/data/realtime/realtime_notifier.h"
#include "components/data/realtime/realtime_thread_pool_manager.h"
#include "components/util/sleepfor_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {
using privacy_sandbox::server_common::GetTracer;
using testing::_;
using testing::Field;
using testing::Return;

class RealtimeThreadPoolNotifierAwsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    privacy_sandbox::server_common::telemetry::TelemetryConfig config_proto;
    config_proto.set_mode(
        privacy_sandbox::server_common::telemetry::TelemetryConfig::PROD);
    kv_server::KVServerContextMap(
        privacy_sandbox::server_common::telemetry::BuildDependentConfig(
            config_proto));
  }
  int32_t thread_number_ = 4;
};

TEST_F(RealtimeThreadPoolNotifierAwsTest, SuccesfullyCreated) {
  std::vector<RealtimeNotifierMetadata> test_metadata;
  for (int i = 0; i < thread_number_; i++) {
    auto change_notifier =
        std::make_unique<MockDeltaFileRecordChangeNotifier>();
    AwsRealtimeNotifierMetadata metadatum = {
        .maybe_sleep_for = std::make_unique<MockSleepFor>(),
        .change_notifier_for_unit_testing = change_notifier.release(),
    };
    test_metadata.push_back(std::move(metadatum));
  }
  NotifierMetadata metadata = AwsNotifierMetadata{};
  auto maybe_pool_manager = RealtimeThreadPoolManager::Create(
      metadata, thread_number_, std::move(test_metadata));
  ASSERT_TRUE(maybe_pool_manager.ok());
}

TEST_F(RealtimeThreadPoolNotifierAwsTest, SuccesfullyStartsAndStops) {
  std::vector<RealtimeNotifierMetadata> test_metadata;
  for (int i = 0; i < thread_number_; i++) {
    auto change_notifier =
        std::make_unique<MockDeltaFileRecordChangeNotifier>();
    AwsRealtimeNotifierMetadata metadatum = {
        .maybe_sleep_for = std::make_unique<MockSleepFor>(),
        .change_notifier_for_unit_testing = change_notifier.release(),
    };
    test_metadata.push_back(std::move(metadatum));
  }
  NotifierMetadata metadata = AwsNotifierMetadata{};
  auto maybe_pool_manager = RealtimeThreadPoolManager::Create(
      metadata, thread_number_, std::move(test_metadata));
  ASSERT_TRUE(maybe_pool_manager.ok());
  absl::Status status = (*maybe_pool_manager)->Start([](const std::string&) {
    return absl::OkStatus();
  });
  ASSERT_TRUE(status.ok());
  status = (*maybe_pool_manager)->Stop();
  ASSERT_TRUE(status.ok());
}
}  // namespace
}  // namespace kv_server
