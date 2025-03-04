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

#include <string>
#include <utility>
#include <vector>

#include "absl/synchronization/notification.h"
#include "components/data/common/mocks.h"
#include "components/data/common/mocks_aws.h"
#include "components/data/realtime/realtime_notifier.h"
#include "components/telemetry/server_definition.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/data_loading/filename_utils.h"
#include "src/util/sleep/sleepfor_mock.h"

using testing::_;
using testing::Field;
using testing::Return;

namespace kv_server {
namespace {

using privacy_sandbox::server_common::GetTracer;
using ::privacy_sandbox::server_common::MockSleepFor;

class RealtimeNotifierAwsTest : public ::testing::Test {
 protected:
  void SetUp() override { kv_server::InitMetricsContextMap(); }
  std::unique_ptr<MockDeltaFileRecordChangeNotifier> change_notifier_ =
      std::make_unique<MockDeltaFileRecordChangeNotifier>();
  std::unique_ptr<MockSleepFor> mock_sleep_for_ =
      std::make_unique<MockSleepFor>();
};

NotificationsContext GetNotificationsContext() {
  auto span = GetTracer()->GetCurrentSpan();
  return NotificationsContext{.scope = opentelemetry::trace::Scope(span)};
}

TEST_F(RealtimeNotifierAwsTest, NotRunning) {
  AwsRealtimeNotifierMetadata options = {
      .change_notifier_for_unit_testing = change_notifier_.release(),
      .maybe_sleep_for = std::move(mock_sleep_for_),
  };
  auto maybe_notifier = RealtimeNotifier::Create({}, std::move(options));
  ASSERT_TRUE(maybe_notifier.ok());
  ASSERT_FALSE((*maybe_notifier)->IsRunning());
}

TEST_F(RealtimeNotifierAwsTest, ConsecutiveStartsWork) {
  AwsRealtimeNotifierMetadata options = {
      .change_notifier_for_unit_testing = change_notifier_.release(),
      .maybe_sleep_for = std::move(mock_sleep_for_),
  };
  auto maybe_notifier = RealtimeNotifier::Create({}, std::move(options));
  absl::Status status = (*maybe_notifier)->Start([](const std::string&) {
    return absl::OkStatus();
  });
  ASSERT_TRUE(status.ok());
  status = (*maybe_notifier)->Start([](const std::string&) {
    return absl::OkStatus();
  });
  ASSERT_FALSE(status.ok());
}

TEST_F(RealtimeNotifierAwsTest, StartsAndStops) {
  AwsRealtimeNotifierMetadata options = {
      .change_notifier_for_unit_testing = change_notifier_.release(),
      .maybe_sleep_for = std::move(mock_sleep_for_),
  };
  auto maybe_notifier = RealtimeNotifier::Create({}, std::move(options));
  absl::Status status = (*maybe_notifier)->Start([](const std::string&) {
    return absl::OkStatus();
  });
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE((*maybe_notifier)->IsRunning());
  status = (*maybe_notifier)->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE((*maybe_notifier)->IsRunning());
}

TEST_F(RealtimeNotifierAwsTest, NotifiesWithHighPriorityUpdates) {
  std::string high_priority_update_1 = "high_priority_update_1";
  std::string high_priority_update_2 = "high_priority_update_2";

  EXPECT_CALL(*change_notifier_, GetNotifications(_, _))
      // x64 encoded file with two records
      .WillOnce([&]() {
        NotificationsContext nc1 = GetNotificationsContext();
        nc1.realtime_messages = std::vector<RealtimeMessage>(
            {RealtimeMessage{.parsed_notification = high_priority_update_1}});

        return nc1;
      })
      // x64 encoded file with one record
      .WillOnce([&]() {
        NotificationsContext nc2 = GetNotificationsContext();
        nc2.realtime_messages = std::vector<RealtimeMessage>(
            {RealtimeMessage{.parsed_notification = high_priority_update_2}});

        return nc2;
      })
      .WillRepeatedly([]() { return GetNotificationsContext(); });

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
  AwsRealtimeNotifierMetadata options = {
      .change_notifier_for_unit_testing = change_notifier_.release(),
      .maybe_sleep_for = std::move(mock_sleep_for_),
  };
  auto maybe_notifier = RealtimeNotifier::Create({}, std::move(options));
  absl::Status status = (*maybe_notifier)->Start(callback.AsStdFunction());
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE((*maybe_notifier)->IsRunning());
  finished.WaitForNotification();
  status = (*maybe_notifier)->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE((*maybe_notifier)->IsRunning());
}

TEST_F(RealtimeNotifierAwsTest, GetChangesFailure) {
  std::string high_priority_update_1 = "high_priority_update_1";
  EXPECT_CALL(*change_notifier_, GetNotifications(_, _))
      .WillOnce([]() { return absl::InvalidArgumentError("stuff"); })
      .WillOnce([]() { return absl::InvalidArgumentError("stuff"); })
      .WillOnce([&]() {
        NotificationsContext nc1 = GetNotificationsContext();
        nc1.realtime_messages = std::vector<RealtimeMessage>(
            {RealtimeMessage{.parsed_notification = high_priority_update_1}});

        return nc1;
      })
      .WillRepeatedly([]() { return GetNotificationsContext(); });

  absl::Notification finished;
  testing::MockFunction<absl::StatusOr<DataLoadingStats>(
      const std::string& record)>
      callback;
  EXPECT_CALL(callback, Call).Times(1).WillOnce([&](const std::string& key) {
    EXPECT_EQ(key, high_priority_update_1);
    finished.Notify();
    return DataLoadingStats{};
  });
  EXPECT_CALL(*mock_sleep_for_, Duration(absl::Seconds(2)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_sleep_for_, Duration(absl::Seconds(4)))
      .Times(1)
      .WillOnce(Return(true));

  AwsRealtimeNotifierMetadata options = {
      .change_notifier_for_unit_testing = change_notifier_.release(),
      .maybe_sleep_for = std::move(mock_sleep_for_),
  };
  auto maybe_notifier = RealtimeNotifier::Create({}, std::move(options));
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
