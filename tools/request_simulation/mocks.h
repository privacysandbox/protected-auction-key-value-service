/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TOOLS_REQUEST_SIMULATION_MOCKS_H_
#define TOOLS_REQUEST_SIMULATION_MOCKS_H_

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "tools/request_simulation/metrics_collector.h"

namespace kv_server {

class MockMetricsCollector : public MetricsCollector {
 public:
  explicit MockMetricsCollector(std::unique_ptr<SleepFor> sleep_for)
      : MetricsCollector(std::move(sleep_for)) {}
  MOCK_METHOD(void, IncrementServerResponseStatusEvent,
              (const absl::Status& status), (override));
  MOCK_METHOD(void, IncrementRequestSentPerInterval, (), (override));
  MOCK_METHOD(void, IncrementRequestsWithOkResponsePerInterval, (), (override));
  MOCK_METHOD(void, IncrementRequestsWithErrorResponsePerInterval, (),
              (override));

  MOCK_METHOD(void, AddLatencyToHistogram, (absl::Duration latency),
              (override));

  MOCK_METHOD(absl::Duration, GetPercentileLatency, (double percentile),
              (override));

  MOCK_METHOD(absl::Status, Start, (), (override));

  MOCK_METHOD(absl::Status, Stop, (), (override));
};

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_MOCKS_H_
