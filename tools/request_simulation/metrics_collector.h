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

#ifndef TOOLS_REQUEST_SIMULATION_METRICS_COLLECTOR_H_
#define TOOLS_REQUEST_SIMULATION_METRICS_COLLECTOR_H_

#include <memory>

#include "absl/flags/flag.h"
#include "components/data/common/thread_manager.h"
#include "components/util/sleepfor.h"
#include "src/cpp/telemetry/metrics_recorder.h"
#include "test/core/util/histogram.h"

namespace kv_server {
// MetricsCollector periodically collects metrics
// periodically prints metrics to the log and publishes metrics to
// MetricsRecorder
class MetricsCollector {
 public:
  MetricsCollector(
      privacy_sandbox::server_common::MetricsRecorder& metrics_recorder,
      std::unique_ptr<SleepFor> sleep_for);
  // Increments server response status event
  virtual void IncrementServerResponseStatusEvent(const absl::Status& status);
  // Increments requests sent counter for the current interval
  virtual void IncrementRequestSentPerInterval();
  // Increments requests with ok response counter for the current interval
  virtual void IncrementRequestsWithOkResponsePerInterval();
  // Increments requests with error response counter for the current interval
  virtual void IncrementRequestsWithErrorResponsePerInterval();
  // Adds latency data point to the histogram
  virtual void AddLatencyToHistogram(absl::Duration latency);
  // Gets percentile latency from histogram
  virtual absl::Duration GetPercentileLatency(double percentile);
  // Starts the metrics collector
  virtual absl::Status Start();
  // Stops the metrics collector
  virtual absl::Status Stop();

  virtual ~MetricsCollector() {
    grpc_histogram_destroy(histogram_per_interval_);
  }

  // MetricsReporter is neither copyable nor movable.
  MetricsCollector(const MetricsCollector&) = delete;
  MetricsCollector& operator=(const MetricsCollector&) = delete;

 private:
  // Prints metrics to the log and publishes metrics to MetricsRecorder,
  // which happens periodically like every 1 min
  void PublishMetrics();
  // Resets requests counters for the current interval to have a fresh
  // start for collecting metrics for the next interval
  void ResetRequestsPerInterval();
  // Resets histogram for the current interval to have a fresh
  // start for collecting latency data points for the next interval
  void ResetHistogram();
  // Gets estimated query per seconds, which is the
  // number of requests with ok response divided by report interval in seconds
  int64_t GetQPS();
  absl::Mutex mutex_;
  mutable std::atomic<int64_t> requests_sent_per_interval_;
  mutable std::atomic<int64_t> requests_with_ok_response_per_interval_;
  mutable std::atomic<int64_t> requests_with_error_response_per_interval_;
  absl::Duration report_interval_;
  std::unique_ptr<TheadManager> report_thread_manager_;
  privacy_sandbox::server_common::MetricsRecorder& metrics_recorder_;
  std::unique_ptr<SleepFor> sleep_for_;
  grpc_histogram* histogram_per_interval_ ABSL_GUARDED_BY(mutex_);
  friend class MetricsCollectorPeer;
};

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_METRICS_COLLECTOR_H_
