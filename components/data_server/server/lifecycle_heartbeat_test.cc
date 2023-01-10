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

class MockInstanceClient : public InstanceClient {
 public:
  MOCK_METHOD(absl::StatusOr<std::string>, GetEnvironmentTag, (),
              (const, override));
  MOCK_METHOD(absl::Status, RecordLifecycleHeartbeat,
              (std::string_view lifecycle_hook_name), (const, override));
  MOCK_METHOD(absl::Status, CompleteLifecycle,
              (std::string_view lifecycle_hook_name), (const, override));
};

class MockParameterFetcher : public ParameterFetcher {
 public:
  MOCK_METHOD(std::string, GetParameter, (std::string_view parameter_suffix),
              (const, override));
};

TEST(LifecycleHeartbeat, CantRunTwice) {
  std::unique_ptr<PeriodicClosure> pc = std::make_unique<FakePeriodicClosure>();
  MockInstanceClient ic;
  std::unique_ptr<LifecycleHeartbeat> lh =
      LifecycleHeartbeat::Create(std::move(pc), ic);
  MockParameterFetcher pf;
  EXPECT_CALL(pf, GetParameter("launch-hook")).WillOnce(testing::Return("hi"));
  absl::Status status = lh->Start(pf);
  ASSERT_TRUE(status.ok());
  status = lh->Start(pf);
  ASSERT_FALSE(status.ok());
  EXPECT_CALL(ic, CompleteLifecycle("hi"))
      .WillOnce(testing::Return(absl::OkStatus()));
}

TEST(LifecycleHeartbeat, RecordsHeartbeat) {
  std::unique_ptr<PeriodicClosure> pc = std::make_unique<FakePeriodicClosure>();
  FakePeriodicClosure* pcp = dynamic_cast<FakePeriodicClosure*>(pc.get());
  MockInstanceClient ic;
  std::unique_ptr<LifecycleHeartbeat> lh =
      LifecycleHeartbeat::Create(std::move(pc), ic);
  MockParameterFetcher pf;
  EXPECT_CALL(pf, GetParameter("launch-hook")).WillOnce(testing::Return("hi"));
  absl::Status status = lh->Start(pf);
  EXPECT_CALL(ic, RecordLifecycleHeartbeat("hi"))
      .WillOnce(testing::Return(absl::OkStatus()));
  pcp->RunFunc();
  ASSERT_TRUE(status.ok());
  EXPECT_CALL(ic, CompleteLifecycle("hi"))
      .WillOnce(testing::Return(absl::OkStatus()));
}

TEST(LifecycleHeartbeat, OnlyFinishOnce) {
  std::unique_ptr<PeriodicClosure> pc = std::make_unique<FakePeriodicClosure>();
  MockInstanceClient ic;
  EXPECT_CALL(ic, CompleteLifecycle("hi"))
      .Times(1)
      .WillOnce(testing::Return(absl::OkStatus()));
  {
    std::unique_ptr<LifecycleHeartbeat> lh =
        LifecycleHeartbeat::Create(std::move(pc), ic);
    MockParameterFetcher pf;
    EXPECT_CALL(pf, GetParameter("launch-hook"))
        .WillOnce(testing::Return("hi"));
    absl::Status status = lh->Start(pf);
    lh->Finish();
  }
}

}  // namespace kv_server
