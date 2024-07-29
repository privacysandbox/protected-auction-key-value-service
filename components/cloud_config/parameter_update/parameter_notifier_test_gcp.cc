// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
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
#include "components/cloud_config/parameter_update/parameter_notifier.h"
#include "components/data/common/mocks.h"
#include "components/util/sleepfor_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::SimulatedSteadyClock;
using testing::_;
using testing::AllOf;
using testing::Field;
using testing::Return;

// TODO(b/356110894) Remove or combine this test with AWS test once
// change_notifier_gcp is ready to use
class ParameterNotifierGCPTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<MockChangeNotifier> mock_change_notifier =
        std::make_unique<MockChangeNotifier>();
    change_notifier_ = mock_change_notifier.get();
    std::unique_ptr<MockSleepFor> mock_sleep_for =
        std::make_unique<MockSleepFor>();
    sleep_for_ = mock_sleep_for.get();
    notifier_ = std::make_unique<ParameterNotifier>(
        std::move(mock_change_notifier), parameter_name_, poll_frequency_,
        std::move(mock_sleep_for), sim_clock_,
        const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
            privacy_sandbox::server_common::log::kNoOpContext));
  }
  std::unique_ptr<ParameterNotifier> notifier_;
  MockChangeNotifier* change_notifier_;
  MockSleepFor* sleep_for_;
  SimulatedSteadyClock sim_clock_;
  absl::Duration poll_frequency_ = absl::Minutes(5);
  std::string parameter_name_ = "test_parameter";
};

TEST_F(ParameterNotifierGCPTest, NotRunning) {
  ASSERT_FALSE(notifier_->IsRunning());
}

TEST_F(ParameterNotifierGCPTest, StartsAndStops) {
  absl::Status status = notifier_->Start<std::string>(
      [](std::string_view param_name) { return "test_value"; },
      [](std::string param_value) {});
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(notifier_->IsRunning());
  status = notifier_->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE(notifier_->IsRunning());
}

TEST_F(ParameterNotifierGCPTest, BackupPollOnly) {
  std::string param_update_triggerred_by_backpoll_1 = "value_1";
  std::string param_update_triggerred_by_backpoll_2 = "value_2";
  std::string param_update_triggerred_by_backpoll_3 = "value_3";
  absl::Notification finished;
  testing::MockFunction<absl::StatusOr<std::string>(
      std::string_view param_name)>
      get_parameter_callback;
  EXPECT_CALL(get_parameter_callback, Call)
      // called from initial poll
      .WillOnce([&](std::string_view param_name) {
        EXPECT_EQ(param_name, parameter_name_);
        return param_update_triggerred_by_backpoll_1;
      })
      .WillOnce([&](std::string_view param_name) {
        EXPECT_EQ(param_name, parameter_name_);
        return param_update_triggerred_by_backpoll_2;
      })
      .WillOnce([&](std::string_view param_name) {
        EXPECT_EQ(param_name, parameter_name_);
        return param_update_triggerred_by_backpoll_3;
      })
      .WillRepeatedly([&](std::string_view param_name) {
        EXPECT_EQ(param_name, parameter_name_);
        return "";
      });
  testing::MockFunction<void(std::string param_value)> apply_parameter_callback;
  EXPECT_CALL(apply_parameter_callback, Call)
      .WillOnce([&](std::string param_value) {
        EXPECT_EQ(param_value, param_update_triggerred_by_backpoll_1);
      })
      .WillOnce([&](std::string param_value) {
        EXPECT_EQ(param_value, param_update_triggerred_by_backpoll_2);
      })
      .WillOnce([&](std::string param_value) {
        EXPECT_EQ(param_value, param_update_triggerred_by_backpoll_3);
        finished.Notify();
      })
      .WillRepeatedly(
          [&](std::string param_value) { EXPECT_EQ(param_value, ""); });
  EXPECT_CALL(*sleep_for_, Duration(_)).WillRepeatedly(Return(true));
  absl::Status status =
      notifier_->Start(get_parameter_callback.AsStdFunction(),
                       apply_parameter_callback.AsStdFunction());
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(notifier_->IsRunning());
  finished.WaitForNotification();
  status = notifier_->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE(notifier_->IsRunning());
}
}  // namespace
}  // namespace kv_server
