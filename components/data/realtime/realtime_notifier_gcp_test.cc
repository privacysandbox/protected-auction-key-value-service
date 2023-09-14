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

#include "absl/strings/escaping.h"
#include "absl/synchronization/notification.h"
#include "components/data/common/mocks.h"
#include "components/data/realtime/realtime_notifier.h"
#include "components/util/sleepfor_mock.h"
#include "gmock/gmock.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/mocks/mock_ack_handler.h"
#include "google/cloud/pubsub/mocks/mock_subscriber_connection.h"
#include "google/cloud/pubsub/subscriber.h"
#include "gtest/gtest.h"
#include "public/data_loading/filename_utils.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

using ::google::cloud::pubsub::AckHandler;
using ::google::cloud::pubsub::MessageBuilder;
using ::google::cloud::pubsub::Subscriber;
using ::google::cloud::pubsub::SubscriberConnection::SubscribeParams;
using ::google::cloud::pubsub_mocks::MockAckHandler;
using ::google::cloud::pubsub_mocks::MockSubscriberConnection;
using privacy_sandbox::server_common::GetTracer;
using privacy_sandbox::server_common::MockMetricsRecorder;
using testing::_;
using testing::Field;
using testing::Return;

class RealtimeNotifierGcpTest : public ::testing::Test {
 protected:
  MockMetricsRecorder mock_metrics_recorder_;
  std::unique_ptr<MockSleepFor> mock_sleep_for_ =
      std::make_unique<MockSleepFor>();
  std::shared_ptr<MockSubscriberConnection> mock_ =
      std::make_shared<MockSubscriberConnection>();
};

TEST_F(RealtimeNotifierGcpTest, NotRunning) {
  auto subscriber = std::make_unique<Subscriber>(Subscriber(mock_));
  GcpRealtimeNotifierMetadata options = {
      .maybe_sleep_for = std::move(mock_sleep_for_),
      .gcp_subscriber_for_unit_testing = subscriber.release(),
  };
  auto maybe_notifier =
      RealtimeNotifier::Create(mock_metrics_recorder_, {}, std::move(options));
  ASSERT_TRUE(maybe_notifier.ok());
  ASSERT_FALSE((*maybe_notifier)->IsRunning());
}

TEST_F(RealtimeNotifierGcpTest, ConsecutiveStartsWork) {
  EXPECT_CALL(*mock_, options);
  EXPECT_CALL(*mock_, Subscribe).WillOnce([&](SubscribeParams const& p) {
    return make_ready_future(google::cloud::Status{});
  });
  auto subscriber = std::make_unique<Subscriber>(Subscriber(mock_));
  GcpRealtimeNotifierMetadata options = {
      .maybe_sleep_for = std::move(mock_sleep_for_),
      .gcp_subscriber_for_unit_testing = subscriber.release(),
  };
  auto maybe_notifier =
      RealtimeNotifier::Create(mock_metrics_recorder_, {}, std::move(options));
  absl::Status status = (*maybe_notifier)->Start([](const std::string&) {
    return absl::OkStatus();
  });
  ASSERT_TRUE(status.ok());
  status = (*maybe_notifier)->Start([](const std::string&) {
    return absl::OkStatus();
  });
  ASSERT_FALSE(status.ok());
}

TEST_F(RealtimeNotifierGcpTest, StartsAndStops) {
  EXPECT_CALL(*mock_, options);
  EXPECT_CALL(*mock_, Subscribe).WillOnce([&](SubscribeParams const& p) {
    return make_ready_future(google::cloud::Status{});
  });

  auto subscriber = std::make_unique<Subscriber>(Subscriber(mock_));
  GcpRealtimeNotifierMetadata options = {
      .maybe_sleep_for = std::move(mock_sleep_for_),
      .gcp_subscriber_for_unit_testing = subscriber.release(),
  };

  auto maybe_notifier =
      RealtimeNotifier::Create(mock_metrics_recorder_, {}, std::move(options));
  absl::Status status = (*maybe_notifier)->Start([](const std::string&) {
    return absl::OkStatus();
  });
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE((*maybe_notifier)->IsRunning());
  status = (*maybe_notifier)->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE((*maybe_notifier)->IsRunning());
}

TEST_F(RealtimeNotifierGcpTest, NotifiesWithHighPriorityUpdates) {
  std::string high_priority_update_1 = "high_priority_update_1";
  std::string high_priority_update_2 = "high_priority_update_2";
  EXPECT_CALL(*mock_, options);
  EXPECT_CALL(*mock_, Subscribe).WillOnce([&](SubscribeParams const& p) {
    {
      auto ack = std::make_unique<MockAckHandler>();
      EXPECT_CALL(*ack, ack()).Times(1);
      auto encoded = absl::Base64Escape(high_priority_update_1);
      p.callback(MessageBuilder{}.SetData(encoded).Build(),
                 AckHandler(std::move(ack)));

      auto ack2 = std::make_unique<MockAckHandler>();
      EXPECT_CALL(*ack2, ack()).Times(1);
      auto encoded2 = absl::Base64Escape(high_priority_update_2);
      p.callback(MessageBuilder{}.SetData(encoded2).Build(),
                 AckHandler(std::move(ack2)));
    }
    return make_ready_future(google::cloud::Status{});
  });

  absl::Notification finished;
  testing::MockFunction<absl::StatusOr<DataLoadingStats>(
      const std::string& record)>
      callback;
  // will match the above
  EXPECT_CALL(callback, Call)
      .Times(2)
      .WillOnce([&](const std::string& key) {
        EXPECT_EQ(key, high_priority_update_1);
        return DataLoadingStats{};
      })
      .WillOnce([&](const std::string& key) {
        EXPECT_EQ(key, high_priority_update_2);
        finished.Notify();
        return DataLoadingStats{};
      });
  auto subscriber = std::make_unique<Subscriber>(Subscriber(mock_));
  GcpRealtimeNotifierMetadata options = {
      .gcp_subscriber_for_unit_testing = subscriber.release(),
      .maybe_sleep_for = std::move(mock_sleep_for_),
  };
  auto maybe_notifier =
      RealtimeNotifier::Create(mock_metrics_recorder_, {}, std::move(options));
  ASSERT_TRUE(maybe_notifier.ok());
  ASSERT_FALSE((*maybe_notifier)->IsRunning());
  absl::Status status = (*maybe_notifier)->Start(callback.AsStdFunction());
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE((*maybe_notifier)->IsRunning());
  finished.WaitForNotification();
  status = (*maybe_notifier)->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE((*maybe_notifier)->IsRunning());
}

}  // namespace
}  // namespace kv_server
