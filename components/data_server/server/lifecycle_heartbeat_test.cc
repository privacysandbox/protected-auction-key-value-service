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

#include "components/data_server/server/lifecycle_heartbeat.h"

#include <string>
#include <utility>
#include <vector>

#include "components/data_server/server/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {

class FakePeriodicClosure : public PeriodicClosure {
 public:
  absl::Status StartNow(absl::Duration interval,
                        std::function<void()> closure) override {
    if (is_running_) {
      return absl::FailedPreconditionError("Already running.");
    }
    closure_ = std::move(closure);
    is_running_ = true;
    return absl::OkStatus();
  }

  absl::Status StartDelayed(absl::Duration interval,
                            std::function<void()> closure) override {
    return StartNow(interval, std::move(closure));
  }

  void Stop() override { is_running_ = false; }

  bool IsRunning() const override { return is_running_; }

  void RunFunc() { closure_(); }

 private:
  bool is_running_ = false;
  std::function<void()> closure_;
};

class LifecycleHeartbeatTest : public ::testing::Test {
 protected:
  void SetUp() override { kv_server::InitMetricsContextMap(); }
};

TEST_F(LifecycleHeartbeatTest, CantRunTwice) {
  std::unique_ptr<PeriodicClosure> periodic_closure =
      std::make_unique<FakePeriodicClosure>();
  MockInstanceClient instance_client;
  std::unique_ptr<LifecycleHeartbeat> lifecycle_heartbeat =
      LifecycleHeartbeat::Create(std::move(periodic_closure), instance_client);
  MockParameterFetcher parameter_fetcher;
  EXPECT_CALL(parameter_fetcher,
              GetParameter("launch-hook", testing::Eq(std::nullopt)))
      .WillOnce(testing::Return("hi"));
  absl::Status status = lifecycle_heartbeat->Start(parameter_fetcher);
  ASSERT_TRUE(status.ok());
  status = lifecycle_heartbeat->Start(parameter_fetcher);
  ASSERT_FALSE(status.ok());
  EXPECT_CALL(instance_client, CompleteLifecycle("hi"))
      .WillOnce(testing::Return(absl::OkStatus()));
}

TEST_F(LifecycleHeartbeatTest, RecordsHeartbeat) {
  std::unique_ptr<PeriodicClosure> periodic_closure =
      std::make_unique<FakePeriodicClosure>();
  FakePeriodicClosure* periodic_closurep =
      dynamic_cast<FakePeriodicClosure*>(periodic_closure.get());
  MockInstanceClient instance_client;
  std::unique_ptr<LifecycleHeartbeat> lifecycle_heartbeat =
      LifecycleHeartbeat::Create(std::move(periodic_closure), instance_client);
  MockParameterFetcher parameter_fetcher;
  EXPECT_CALL(parameter_fetcher,
              GetParameter("launch-hook", testing::Eq(std::nullopt)))
      .WillOnce(testing::Return("hi"));
  absl::Status status = lifecycle_heartbeat->Start(parameter_fetcher);
  ASSERT_TRUE(status.ok());
  EXPECT_CALL(instance_client, RecordLifecycleHeartbeat("hi"))
      .WillOnce(testing::Return(absl::OkStatus()));
  EXPECT_CALL(instance_client, CompleteLifecycle("hi"))
      .WillOnce(testing::Return(absl::OkStatus()));
  periodic_closurep->RunFunc();
}

TEST_F(LifecycleHeartbeatTest, OnlyFinishOnce) {
  std::unique_ptr<PeriodicClosure> periodic_closure =
      std::make_unique<FakePeriodicClosure>();
  MockInstanceClient instance_client;
  EXPECT_CALL(instance_client, CompleteLifecycle("hi"))
      .Times(1)
      .WillOnce(testing::Return(absl::OkStatus()));
  std::unique_ptr<LifecycleHeartbeat> lifecycle_heartbeat =
      LifecycleHeartbeat::Create(std::move(periodic_closure), instance_client);
  MockParameterFetcher parameter_fetcher;
  EXPECT_CALL(parameter_fetcher,
              GetParameter("launch-hook", testing::Eq(std::nullopt)))
      .WillOnce(testing::Return("hi"));
  absl::Status status = lifecycle_heartbeat->Start(parameter_fetcher);
  lifecycle_heartbeat->Finish();
}

}  // namespace kv_server
