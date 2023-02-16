/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_TELEMETRY_MOCKS_H_
#define COMPONENTS_TELEMETRY_MOCKS_H_

#include <string>

#include "components/telemetry/metrics_recorder.h"
#include "gmock/gmock.h"

namespace kv_server {

class MockMetricsRecorder : public MetricsRecorder {
 public:
  MOCK_METHOD(void, IncrementEventStatus,
              (std::string event, absl::Status status, uint64_t count),
              (override));
  MOCK_METHOD(void, IncrementEventCounter, (std::string name), (override));
};

}  // namespace kv_server

#endif  // COMPONENTS_TELEMETRY_MOCKS_H_
