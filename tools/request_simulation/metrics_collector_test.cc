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

#include "tools/request_simulation/metrics_collector.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/util/sleepfor_mock.h"
#include "gtest/gtest.h"
#include "src/telemetry/mocks.h"

namespace kv_server {

using privacy_sandbox::server_common::MockMetricsRecorder;
using privacy_sandbox::server_common::SimulatedSteadyClock;
using privacy_sandbox::server_common::SteadyTime;
using testing::_;
using testing::Return;

class MetricsCollectorPeer {
 public:
  MetricsCollectorPeer() = delete;
  static int64_t ReadRequestsSentPerInterval(const MetricsCollector& c) {
    return c.requests_sent_per_interval_.load(std::memory_order_relaxed);
  }
  static int64_t ReadRequestsWithOkResponsePerInterval(
      const MetricsCollector& c) {
    return c.requests_with_ok_response_per_interval_.load(
        std::memory_order_relaxed);
  }
  static int64_t ReadRequestsWithErrorResponsePerInterval(
      const MetricsCollector& c) {
    return c.requests_with_error_response_per_interval_.load(
        std::memory_order_relaxed);
  }
  static int64_t ReadQPS(MetricsCollector& c) { return c.GetQPS(); }

  static void ResetStats(MetricsCollector& c) {
    c.ResetRequestsPerInterval();
    c.ResetHistogram();
  }
};

namespace {
class MetricsCollectorTest : public ::testing::Test {
 protected:
  MetricsCollectorTest() {
    metrics_collector_ =
        std::make_unique<MetricsCollector>(std::make_unique<MockSleepFor>());
  }
  SimulatedSteadyClock sim_clock_;
  std::unique_ptr<MetricsCollector> metrics_collector_;
};

TEST_F(MetricsCollectorTest, TestCollectMetrics) {
  for (int i = 0; i < 60; i++) {
    metrics_collector_->IncrementRequestSentPerInterval();
    metrics_collector_->IncrementRequestsWithOkResponsePerInterval();
  }
  metrics_collector_->IncrementRequestsWithErrorResponsePerInterval();
  EXPECT_EQ(
      MetricsCollectorPeer::ReadRequestsSentPerInterval(*metrics_collector_),
      60);
  EXPECT_EQ(MetricsCollectorPeer::ReadRequestsWithOkResponsePerInterval(
                *metrics_collector_),
            60);
  EXPECT_EQ(MetricsCollectorPeer::ReadRequestsWithErrorResponsePerInterval(
                *metrics_collector_),
            1);
  EXPECT_EQ(MetricsCollectorPeer::ReadQPS(*metrics_collector_), 1);
  // Reset stats
  MetricsCollectorPeer::ResetStats(*metrics_collector_);
  for (int i = 0; i < 10; i++) {
    metrics_collector_->IncrementRequestSentPerInterval();
    metrics_collector_->IncrementRequestsWithOkResponsePerInterval();
  }
  EXPECT_EQ(
      MetricsCollectorPeer::ReadRequestsSentPerInterval(*metrics_collector_),
      10);
  EXPECT_EQ(MetricsCollectorPeer::ReadRequestsWithOkResponsePerInterval(
                *metrics_collector_),
            10);
}

}  // namespace
}  // namespace kv_server
