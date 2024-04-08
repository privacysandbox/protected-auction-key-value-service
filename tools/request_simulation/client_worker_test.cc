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

#include "tools/request_simulation/client_worker.h"

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
#include "tools/request_simulation/request/raw_request.pb.h"
#include "tools/request_simulation/request_generation_util.h"

namespace kv_server {

using privacy_sandbox::server_common::SimulatedSteadyClock;
using privacy_sandbox::server_common::SteadyTime;
using testing::_;
using testing::Return;

namespace {

void PrefillMessageQueue(MessageQueue& queue, int num_of_messages,
                         std::string_view key) {
  for (int i = 0; i < num_of_messages; ++i) {
    queue.Push(std::string(key));
  }
}

class ClientWorkerTest : public ::testing::Test {
 protected:
  ClientWorkerTest() {
    absl::flat_hash_map<std::string, std::string> data_map = {{"key", "value"}};
    fake_get_value_service_ =
        std::make_unique<FakeKeyValueServiceImpl>(data_map);
    grpc::ServerBuilder builder;
    builder.RegisterService(fake_get_value_service_.get());
    server_ = (builder.BuildAndStart());
    sleep_for_ = std::make_unique<MockSleepFor>();
    sleep_for_metrics_collector_ = std::make_unique<MockSleepFor>();
  }

  ~ClientWorkerTest() override {
    server_->Shutdown();
    server_->Wait();
  }
  std::unique_ptr<FakeKeyValueServiceImpl> fake_get_value_service_;
  std::unique_ptr<grpc::Server> server_;
  SimulatedSteadyClock sim_clock_;
  std::unique_ptr<MockSleepFor> sleep_for_metrics_collector_;
  std::unique_ptr<MockSleepFor> sleep_for_;
};

TEST_F(ClientWorkerTest, SingleClientWorkerTest) {
  std::string key("key");
  std::string method("/kv_server.v2.KeyValueService/GetValuesHttp");
  auto request_converter = [](const std::string& request_body) {
    RawRequest request;
    request.mutable_raw_body()->set_data(request_body);
    return request;
  };

  MessageQueue message_queue(10000);
  int num_of_messages_prefill = 1500;
  PrefillMessageQueue(message_queue, num_of_messages_prefill, key);

  int requests_per_second = 1000;
  EXPECT_CALL(*sleep_for_, Duration(_)).WillRepeatedly(Return(true));
  RateLimiter rate_limiter(0, requests_per_second, sim_clock_,
                           std::move(sleep_for_), absl::Seconds(0));
  EXPECT_CALL(*sleep_for_metrics_collector_, Duration(_))
      .WillRepeatedly(Return(true));
  std::unique_ptr<MockMetricsCollector> metrics_collector =
      std::make_unique<MockMetricsCollector>(
          std::move(sleep_for_metrics_collector_));
  EXPECT_CALL(*metrics_collector, IncrementServerResponseStatusEvent(_))
      .Times(requests_per_second);
  EXPECT_CALL(*metrics_collector, IncrementRequestSentPerInterval())
      .Times(requests_per_second);
  EXPECT_CALL(*metrics_collector, IncrementRequestsWithOkResponsePerInterval())
      .Times(requests_per_second);
  EXPECT_CALL(*metrics_collector, AddLatencyToHistogram(_))
      .Times(requests_per_second);
  auto worker =
      std::make_unique<ClientWorker<RawRequest, google::api::HttpBody>>(
          0, server_->InProcessChannel(grpc::ChannelArguments()), method,
          absl::Seconds(1), request_converter, message_queue, rate_limiter,
          *metrics_collector, false);
  sim_clock_.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(worker->Start().ok());
  EXPECT_TRUE(worker->IsRunning());
  absl::SleepFor(absl::Seconds(1));
  EXPECT_TRUE(worker->Stop().ok());
  EXPECT_EQ(message_queue.Size(), 500);
}

TEST_F(ClientWorkerTest, MultipleClientWorkersTest) {
  std::string key("key");
  std::string method("/kv_server.v2.KeyValueService/GetValuesHttp");
  auto request_converter = [](const std::string& request_body) {
    RawRequest request;
    request.mutable_raw_body()->set_data(request_body);
    return request;
  };

  MessageQueue message_queue(10000);
  int num_of_messages_prefill = 1500;
  PrefillMessageQueue(message_queue, num_of_messages_prefill, key);

  int requests_per_second = 1000;
  EXPECT_CALL(*sleep_for_, Duration(_)).WillRepeatedly(Return(true));
  RateLimiter rate_limiter(0, requests_per_second, sim_clock_,
                           std::move(sleep_for_), absl::Seconds(0));
  EXPECT_CALL(*sleep_for_metrics_collector_, Duration(_))
      .WillRepeatedly(Return(true));
  std::unique_ptr<MockMetricsCollector> metrics_collector =
      std::make_unique<MockMetricsCollector>(
          std::move(sleep_for_metrics_collector_));
  EXPECT_CALL(*metrics_collector, IncrementServerResponseStatusEvent(_))
      .Times(requests_per_second);
  EXPECT_CALL(*metrics_collector, IncrementRequestSentPerInterval())
      .Times(requests_per_second);
  EXPECT_CALL(*metrics_collector, IncrementRequestsWithOkResponsePerInterval())
      .Times(requests_per_second);
  EXPECT_CALL(*metrics_collector, AddLatencyToHistogram(_))
      .Times(requests_per_second);
  sim_clock_.AdvanceTime(absl::Seconds(1));
  int num_of_workers =
      std::min(50, (int)std::thread::hardware_concurrency() - 1);
  std::vector<std::unique_ptr<ClientWorker<RawRequest, google::api::HttpBody>>>
      workers;
  for (int i = 0; i < num_of_workers; i++) {
    auto worker =
        std::make_unique<ClientWorker<RawRequest, google::api::HttpBody>>(
            i, server_->InProcessChannel(grpc::ChannelArguments()), method,
            absl::Seconds(1), request_converter, message_queue, rate_limiter,
            *metrics_collector, false);
    EXPECT_TRUE(worker->Start().ok());
    EXPECT_TRUE(worker->IsRunning());
    workers.push_back(std::move(worker));
  }
  absl::SleepFor(absl::Seconds(1));
  for (const auto& worker : workers) {
    EXPECT_TRUE(worker->Stop().ok());
  }
  EXPECT_EQ(message_queue.Size(), 500);
}
}  // namespace

}  // namespace kv_server
