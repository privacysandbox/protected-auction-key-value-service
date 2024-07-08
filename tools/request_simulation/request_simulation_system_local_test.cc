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
#include "src/telemetry/mocks.h"
#include "src/util/duration.h"
#include "tools/request_simulation/mocks.h"
#include "tools/request_simulation/request_simulation_system.h"

namespace kv_server {

using privacy_sandbox::server_common::MockMetricsRecorder;
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
  static void PrefillMessageQueue(const RequestSimulationSystem& system,
                                  int number_of_messages) {
    for (int i = 0; i < number_of_messages; i++) {
      system.message_queue_->Push("key");
    }
  }
};

class MockRequestSimulationParameterFetcher
    : public RequestSimulationParameterFetcher {
 public:
  MOCK_METHOD(NotifierMetadata, GetBlobStorageNotifierMetadata, (), (const));
  MOCK_METHOD(NotifierMetadata, GetRealtimeNotifierMetadata, (), (const));
};

namespace {

class SimulationSystemTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::error_code error_code;
    std::filesystem::create_directories(::testing::TempDir(), error_code);
    if (error_code) {
      LOG(WARNING) << error_code.message();
    }
  }

  void TearDown() override {
    std::error_code error_code;
    std::filesystem::remove_all(::testing::TempDir(), error_code);
    if (error_code) {
      LOG(WARNING) << error_code.message();
    }
  }

  SimulationSystemTest() {
    absl::flat_hash_map<std::string, std::string> data_map = {{"key", "value"}};
    fake_get_value_service_ =
        std::make_unique<FakeKeyValueServiceImpl>(data_map);
    grpc::ServerBuilder builder;
    builder.RegisterService(fake_get_value_service_.get());
    server_ = (builder.BuildAndStart());
    sleep_for_request_generator_ = std::make_unique<MockSleepFor>();
    sleep_for_request_generator_rate_limiter_ =
        std::make_unique<MockSleepFor>();
    sleep_for_client_worker_rate_limiter_ = std::make_unique<MockSleepFor>();
    sleep_for_metrics_collector_ = std::make_unique<MockSleepFor>();
    mock_request_simulation_parameter_fetcher_ =
        std::make_unique<MockRequestSimulationParameterFetcher>();
  }

  ~SimulationSystemTest() override {
    server_->Shutdown();
    server_->Wait();
  }
  std::unique_ptr<FakeKeyValueServiceImpl> fake_get_value_service_;
  std::unique_ptr<grpc::Server> server_;
  //  std::unique_ptr<MockMetricsCollector> metrics_collector_;
  SimulatedSteadyClock sim_clock_;
  std::unique_ptr<MockSleepFor> sleep_for_request_generator_;
  std::unique_ptr<MockSleepFor> sleep_for_request_generator_rate_limiter_;
  std::unique_ptr<MockSleepFor> sleep_for_client_worker_rate_limiter_;
  std::unique_ptr<MockSleepFor> sleep_for_metrics_collector_;
  std::unique_ptr<MockRequestSimulationParameterFetcher>
      mock_request_simulation_parameter_fetcher_;
};

TEST_F(SimulationSystemTest, TestSimulationSystemRunning) {
  // Send message at 1000 qps
  absl::SetFlag(&FLAGS_rps, 1000);
  // Generate message at 2500 qps
  absl::SetFlag(&FLAGS_synthetic_requests_fill_qps, 2500);
  // Set the number of client workers to 2
  absl::SetFlag(&FLAGS_concurrency, 2);
  absl::SetFlag(&FLAGS_delta_file_bucket, ::testing::TempDir());
  absl::SetFlag(
      &FLAGS_synthetic_requests_generator_rate_limiter_acquire_timeout,
      absl::Seconds(0));
  absl::SetFlag(&FLAGS_client_worker_rate_limiter_acquire_timeout,
                absl::Seconds(0));
  absl::SetFlag(
      &FLAGS_synthetic_requests_generator_rate_limiter_initial_permits, 0);
  absl::SetFlag(&FLAGS_client_worker_rate_limiter_initial_permits, 0);
  absl::SetFlag(&FLAGS_server_address, "test");
  absl::SetFlag(&FLAGS_is_client_channel, false);
  absl::SetFlag(&FLAGS_delta_file_bucket, ::testing::TempDir());
  auto channel_creation_fn = [this](const std::string& server_address,
                                    const GrpcAuthenticationMode& auth_mode) {
    return server_->InProcessChannel(grpc::ChannelArguments());
  };

  EXPECT_CALL(*sleep_for_request_generator_, Duration(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*sleep_for_client_worker_rate_limiter_, Duration(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*sleep_for_request_generator_rate_limiter_, Duration(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*sleep_for_metrics_collector_, Duration(_))
      .WillRepeatedly(Return(true));

  auto metrics_collector = std::make_unique<MockMetricsCollector>(
      std::move(sleep_for_metrics_collector_));

  EXPECT_CALL(*metrics_collector, Start())
      .WillRepeatedly(Return(absl::OkStatus()));
  EXPECT_CALL(*metrics_collector, Stop())
      .WillRepeatedly(Return(absl::OkStatus()));
  EXPECT_CALL(*metrics_collector, IncrementServerResponseStatusEvent(_))
      .Times(1000);
  EXPECT_CALL(*metrics_collector, IncrementRequestSentPerInterval())
      .Times(1000);
  EXPECT_CALL(*metrics_collector, IncrementRequestsWithOkResponsePerInterval())
      .Times(500);
  EXPECT_CALL(*metrics_collector,
              IncrementRequestsWithErrorResponsePerInterval())
      .Times(500);
  EXPECT_CALL(*metrics_collector, AddLatencyToHistogram(_)).Times(500);

  EXPECT_CALL(*mock_request_simulation_parameter_fetcher_,
              GetBlobStorageNotifierMetadata())
      .WillRepeatedly(Return(
          LocalNotifierMetadata{.local_directory = ::testing::TempDir()}));
  EXPECT_CALL(*mock_request_simulation_parameter_fetcher_,
              GetRealtimeNotifierMetadata())
      .WillRepeatedly(Return(
          LocalNotifierMetadata{.local_directory = ::testing::TempDir()}));
  RequestSimulationSystem system(
      sim_clock_, channel_creation_fn,
      std::move(mock_request_simulation_parameter_fetcher_));
  EXPECT_TRUE(system
                  .Init(std::move(sleep_for_request_generator_),
                        std::move(sleep_for_request_generator_rate_limiter_),
                        std::move(sleep_for_client_worker_rate_limiter_),
                        std::move(metrics_collector))
                  .ok());
  RequestSimulationSystemTestPeer::PrefillMessageQueue(system, 500);
  sim_clock_.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(system.Start().ok());
  EXPECT_TRUE(system.IsRunning());
  absl::SleepFor(absl::Seconds(2));
  EXPECT_TRUE(system.Stop().ok());
  EXPECT_FALSE(system.IsRunning());
  absl::SleepFor(absl::Seconds(1));
  EXPECT_EQ(RequestSimulationSystemTestPeer::ReadMessageQueueSize(system),
            2000);
}
}  // namespace

}  // namespace kv_server
