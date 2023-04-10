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

#ifndef COMPONENTS_TELEMETRY_METRICS_RECORDER_H_
#define COMPONENTS_TELEMETRY_METRICS_RECORDER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "components/util/duration.h"

namespace kv_server {
class MetricsRecorder {
 public:
  // `InitMetrics` must be called prior to `Create`.
  static std::unique_ptr<MetricsRecorder> Create(std::string service_name,
                                                 std::string build_version);

  virtual ~MetricsRecorder() = default;

  // Records `event` with the given `status`.  `count` is the number of times it
  // happened.
  virtual void IncrementEventStatus(std::string event, absl::Status status,
                                    uint64_t count = 1) = 0;

  // Increments a named counter
  virtual void IncrementEventCounter(std::string event) = 0;

  // Register a histogram. This allows fine tuning histogram buckets, units.
  virtual void RegisterHistogram(
      std::string event, std::string description, std::string unit,
      std::vector<double> bucket_boundaries = {}) = 0;

  // Records a value for the `event`.
  // Note that this histogram must be initialized first by calling
  // `RegisterHistogram` above, otherwise, this is a noop.
  virtual void RecordHistogramEvent(std::string event, int64_t value) = 0;

  // Records a latency for a given `event` in the standard catch all latencies
  // histogram with predefined buckets.
  virtual void RecordLatency(std::string event, absl::Duration duration) = 0;
};

// Measures and records the latency of a block of code. Latency is automatically
// recorded to the MetricsRecorder when an instance of this class goes out of
// scope.
class ScopeLatencyRecorder {
 public:
  ScopeLatencyRecorder(std::string event_name,
                       MetricsRecorder& metrics_recorder);
  ~ScopeLatencyRecorder();
  // Returns the latency so far.
  absl::Duration GetLatency();

 private:
  Stopwatch stop_watch_;
  std::string event_name_;
  MetricsRecorder& metrics_recorder_;
};

}  // namespace kv_server
#endif  // COMPONENTS_TELEMETRY_METRICS_RECORDER_H_
