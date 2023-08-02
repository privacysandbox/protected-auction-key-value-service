// Copyright 2023 Google LLC
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

#include "tools/request_simulation/request_simulation_system.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/util/sleepfor_mock.h"
#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "public/testing/fake_key_value_service_impl.h"
#include "src/cpp/util/duration.h"

namespace kv_server {

using privacy_sandbox::server_common::SimulatedSteadyClock;
using privacy_sandbox::server_common::SteadyTime;
using testing::_;
using testing::Return;

class RequestSimulationSystemTestPeer {
 public:
  RequestSimulationSystemTestPeer() = delete;
  static size_t ReadMessageQueueSize(const RequestSimulationSystem& system) {
    return system.message_queue_->Size();
  }
};

namespace {

class SimulationSystemTest : public ::testing::Test {
 protected:
  SimulationSystemTest() {
    absl::flat_hash_map<std::string, std::string> data_map = {{"key", "value"}};
    fake_get_value_service_ =
        std::make_unique<FakeKeyValueServiceImpl>(data_map);
    grpc::ServerBuilder builder;
    builder.RegisterService(fake_get_value_service_.get());
    server_ = (builder.BuildAndStart());
    mock_sleep_for_ = std::make_unique<MockSleepFor>();
  }

  ~SimulationSystemTest() override {
    server_->Shutdown();
    server_->Wait();
  }
  std::unique_ptr<FakeKeyValueServiceImpl> fake_get_value_service_;
  std::unique_ptr<grpc::Server> server_;
  SimulatedSteadyClock sim_clock_;
  std::unique_ptr<MockSleepFor> mock_sleep_for_;
};

TEST_F(SimulationSystemTest, TestSimulationSystemRunning) {
  // Send message at 1000 qps
  absl::SetFlag(&FLAGS_rps, 1000);
  // Generate message at 2500 qps
  absl::SetFlag(&FLAGS_synthetic_requests_fill_qps, 2500);
  // Set the number of client workers to 2
  absl::SetFlag(&FLAGS_concurrency, 2);
  absl::SetFlag(&FLAGS_rate_limiter_permits_acquire_timeout, absl::Seconds(0));
  absl::SetFlag(&FLAGS_server_address, "test");
  auto channel_creation_fn = [this](const std::string& server_address,
                                    const GrpcAuthenticationMode& auth_mode) {
    return server_->InProcessChannel(grpc::ChannelArguments());
  };
  EXPECT_CALL(*mock_sleep_for_, Duration(_)).WillRepeatedly(Return(true));
  RequestSimulationSystem system(sim_clock_, std::move(mock_sleep_for_),
                                 channel_creation_fn);
  EXPECT_TRUE(system.Init().ok());
  sim_clock_.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(system.Start().ok());
  EXPECT_TRUE(system.IsRunning());
  absl::SleepFor(absl::Seconds(1));
  EXPECT_TRUE(system.Stop().ok());
  EXPECT_FALSE(system.IsRunning());
  EXPECT_EQ(RequestSimulationSystemTestPeer::ReadMessageQueueSize(system),
            1500);
}
}  // namespace

}  // namespace kv_server
