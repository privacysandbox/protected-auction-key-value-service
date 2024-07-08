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

#include "tools/request_simulation/synthetic_request_generator.h"

#include "components/util/sleepfor_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tools/request_simulation/request_generation_util.h"

namespace kv_server {
using privacy_sandbox::server_common::SimulatedSteadyClock;
using privacy_sandbox::server_common::SteadyTime;
using testing::_;
using testing::Return;
namespace {

class TestSyntheticRequestGeneratorTest : public ::testing::Test {
 protected:
  SimulatedSteadyClock sim_clock_;
  std::unique_ptr<MockSleepFor> sleep_for_rate_limiter_ =
      std::make_unique<MockSleepFor>();
  std::unique_ptr<MockSleepFor> sleep_for_request_generator_ =
      std::make_unique<MockSleepFor>();
};

TEST_F(TestSyntheticRequestGeneratorTest, TestGenerateRequestsAtFixedRate) {
  int num_of_keys = 10;
  int key_size = 3;
  MessageQueue message_queue(10000);
  int requests_per_second = 1000;
  EXPECT_CALL(*sleep_for_rate_limiter_, Duration(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*sleep_for_request_generator_, Duration(_))
      .WillRepeatedly(Return(true));
  RateLimiter rate_limiter(0, requests_per_second, sim_clock_,
                           std::move(sleep_for_rate_limiter_),
                           absl::Seconds(0));
  SyntheticRequestGenOption option;
  option.number_of_keys_per_request = num_of_keys;
  option.key_size_in_bytes = key_size;
  SyntheticRequestGenerator request_generator(
      message_queue, rate_limiter, std::move(sleep_for_request_generator_),
      requests_per_second, [option]() {
        const auto keys = kv_server::GenerateRandomKeys(
            option.number_of_keys_per_request, option.key_size_in_bytes);
        return kv_server::CreateKVDSPRequestBodyInJson(keys, "debug_token");
      });
  sim_clock_.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(request_generator.Start().ok());
  absl::SleepFor(absl::Seconds(1));
  EXPECT_TRUE(request_generator.IsRunning());
  EXPECT_TRUE(request_generator.Stop().ok());
  EXPECT_EQ(message_queue.Size(), 1000);
}

TEST_F(TestSyntheticRequestGeneratorTest,
       TestGenerateRequestsCappedByCapacity) {
  int num_of_keys = 10;
  int key_size = 3;
  int capacity = 5;
  MessageQueue message_queue(capacity);
  int requests_per_second = 100;
  EXPECT_CALL(*sleep_for_rate_limiter_, Duration(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*sleep_for_request_generator_, Duration(_))
      .WillRepeatedly(Return(true));
  RateLimiter rate_limiter(10, requests_per_second, sim_clock_,
                           std::move(sleep_for_rate_limiter_),
                           absl::Seconds(0));
  SyntheticRequestGenerator request_generator(
      message_queue, rate_limiter, std::move(sleep_for_request_generator_),
      requests_per_second, [num_of_keys, key_size]() {
        const auto keys = kv_server::GenerateRandomKeys(num_of_keys, key_size);
        return kv_server::CreateKVDSPRequestBodyInJson(keys, "debug_token");
      });
  sim_clock_.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(request_generator.Start().ok());
  absl::SleepFor(absl::Seconds(1));
  EXPECT_TRUE(request_generator.Stop().ok());
  EXPECT_FALSE(request_generator.IsRunning());
  EXPECT_EQ(message_queue.Size(), capacity);
}
}  // namespace
}  // namespace kv_server
