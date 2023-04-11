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

#include "components/data/realtime/realtime_notifier.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/synchronization/notification.h"
#include "components/data/common/mocks.h"
#include "components/errors/mocks.h"
#include "components/telemetry/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/data_loading/filename_utils.h"

using testing::_;
using testing::Field;
using testing::Return;

namespace kv_server {
namespace {

class RealtimeNotifierTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<MockSleepFor> mock_sleep_for =
        std::make_unique<MockSleepFor>();
    sleep_for_ = mock_sleep_for.get();
    notifier_ = RealtimeNotifier::Create(mock_metrics_recorder_,
                                         std::move(mock_sleep_for));
  }

  std::unique_ptr<RealtimeNotifier> notifier_;
  MockDeltaFileRecordChangeNotifier change_notifier_;
  MockMetricsRecorder mock_metrics_recorder_;
  MockSleepFor* sleep_for_;
};

NotificationsContext GetNotificationsContext() {
  auto span = GetTracer()->GetCurrentSpan();
  return NotificationsContext{.scope = opentelemetry::trace::Scope(span)};
}

TEST_F(RealtimeNotifierTest, NotRunning) {
  ASSERT_FALSE(notifier_->IsRunning());
}

TEST_F(RealtimeNotifierTest, ConsecutiveStartsWork) {
  absl::Status status = notifier_->Start(
      change_notifier_, [](const std::string&) { return absl::OkStatus(); });
  ASSERT_TRUE(status.ok());
  status = notifier_->Start(
      change_notifier_, [](const std::string&) { return absl::OkStatus(); });
  ASSERT_FALSE(status.ok());
}

TEST_F(RealtimeNotifierTest, StartsAndStops) {
  absl::Status status = notifier_->Start(
      change_notifier_, [](const std::string&) { return absl::OkStatus(); });
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(notifier_->IsRunning());
  status = notifier_->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE(notifier_->IsRunning());
}

TEST_F(RealtimeNotifierTest, NotifiesWithHighPriorityUpdates) {
  std::string high_priority_update_1 = "high_priority_update_1";
  std::string high_priority_update_2 = "high_priority_update_2";

  EXPECT_CALL(change_notifier_, GetNotifications(_, _))
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

  absl::Status status =
      notifier_->Start(change_notifier_, callback.AsStdFunction());
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(notifier_->IsRunning());
  finished.WaitForNotification();
  status = notifier_->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE(notifier_->IsRunning());
}

TEST_F(RealtimeNotifierTest, GetChangesFailure) {
  std::string high_priority_update_1 = "high_priority_update_1";
  EXPECT_CALL(change_notifier_, GetNotifications(_, _))
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
  EXPECT_CALL(*sleep_for_, Duration(absl::Seconds(2)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*sleep_for_, Duration(absl::Seconds(4)))
      .Times(1)
      .WillOnce(Return(true));

  absl::Status status =
      notifier_->Start(change_notifier_, callback.AsStdFunction());
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(notifier_->IsRunning());
  finished.WaitForNotification();
  status = notifier_->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE(notifier_->IsRunning());
}

}  // namespace
}  // namespace kv_server
