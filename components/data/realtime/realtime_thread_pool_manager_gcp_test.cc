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
#include "components/data/realtime/realtime_notifier.h"
#include "components/data/realtime/realtime_thread_pool_manager.h"
#include "components/telemetry/server_definition.h"
#include "gmock/gmock.h"
#include "google/cloud/pubsub/mocks/mock_subscriber_connection.h"
#include "google/cloud/pubsub/subscriber.h"
#include "gtest/gtest.h"
#include "src/util/sleep/sleepfor_mock.h"

namespace kv_server {
namespace {

using ::google::cloud::pubsub::Subscriber;
using ::google::cloud::pubsub::SubscriberConnection;
using ::google::cloud::pubsub_mocks::MockSubscriberConnection;
using privacy_sandbox::server_common::GetTracer;
using ::privacy_sandbox::server_common::MockSleepFor;
using testing::_;
using testing::Field;
using testing::Return;

class RealtimeThreadPoolNotifierGcpTest : public ::testing::Test {
 protected:
  void SetUp() override { kv_server::InitMetricsContextMap(); }
  int32_t thread_number_ = 4;
  std::unique_ptr<MockSleepFor> mock_sleep_for_ =
      std::make_unique<MockSleepFor>();
  std::shared_ptr<MockSubscriberConnection> mock_ =
      std::make_shared<MockSubscriberConnection>();
};

TEST_F(RealtimeThreadPoolNotifierGcpTest, SuccessfullyCreated) {
  auto subscriber = std::make_unique<Subscriber>(Subscriber(mock_));
  GcpRealtimeNotifierMetadata option = GcpRealtimeNotifierMetadata{
      .gcp_subscriber_for_unit_testing = subscriber.release(),
      .maybe_sleep_for = std::move(mock_sleep_for_),
  };
  std::vector<RealtimeNotifierMetadata> options;
  options.push_back(std::move(option));
  NotifierMetadata metadata = GcpNotifierMetadata{};
  auto maybe_pool_manager = RealtimeThreadPoolManager::Create(
      metadata, thread_number_, std::move(options));

  ASSERT_TRUE(maybe_pool_manager.ok());
}

TEST_F(RealtimeThreadPoolNotifierGcpTest, SuccessfullyStartsAndStops) {
  EXPECT_CALL(*mock_, options);
  EXPECT_CALL(*mock_, Subscribe)
      .WillOnce([&](SubscriberConnection::SubscribeParams const& p) {
        return make_ready_future(google::cloud::Status{});
      });
  auto subscriber = std::make_unique<Subscriber>(Subscriber(mock_));
  GcpRealtimeNotifierMetadata option = GcpRealtimeNotifierMetadata{
      .gcp_subscriber_for_unit_testing = subscriber.release(),
      .maybe_sleep_for = std::move(mock_sleep_for_),
  };
  std::vector<RealtimeNotifierMetadata> options;
  options.push_back(std::move(option));
  NotifierMetadata metadata = GcpNotifierMetadata{};
  auto maybe_pool_manager = RealtimeThreadPoolManager::Create(
      metadata, thread_number_, std::move(options));
  absl::Status status = (*maybe_pool_manager)->Start([](const std::string&) {
    return absl::OkStatus();
  });
  ASSERT_TRUE(status.ok());
  status = (*maybe_pool_manager)->Stop();
  ASSERT_TRUE(status.ok());
}
}  // namespace
}  // namespace kv_server
