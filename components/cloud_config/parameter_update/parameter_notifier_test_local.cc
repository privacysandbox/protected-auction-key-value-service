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

// The runtime parameter notification does not support local platform,
// we only need to test start and stop here.

class ParameterNotifierLocalTest : public ::testing::Test {
 protected:
  void SetUp() override {
    notifier_ = std::make_unique<ParameterNotifier>(
        nullptr, std::move(parameter_name_), poll_frequency_,
        std::make_unique<SleepFor>(), sim_clock_,
        const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
            privacy_sandbox::server_common::log::kNoOpContext));
  }
  std::unique_ptr<ParameterNotifier> notifier_;
  MockSleepFor* sleep_for_;
  SimulatedSteadyClock sim_clock_;
  absl::Duration poll_frequency_ = absl::Minutes(5);
  std::string parameter_name_ = "test_parameter";
};

TEST_F(ParameterNotifierLocalTest, NotRunningSmokeTest) {
  ASSERT_FALSE(notifier_->IsRunning());
}

TEST_F(ParameterNotifierLocalTest, StartsAndStopsSmokeTest) {
  absl::Status status = notifier_->Start<std::string>(
      [](std::string_view param_name) { return "test_value"; },
      [](std::string param_value) {});
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(notifier_->IsRunning());
  status = notifier_->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE(notifier_->IsRunning());
}
}  // namespace
}  // namespace kv_server
