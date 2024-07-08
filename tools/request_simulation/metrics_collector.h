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
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "components/data/common/thread_manager.h"
#include "components/util/sleepfor.h"
#include "test/core/util/histogram.h"

namespace kv_server {
inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kServerResponseStatus(
        "ServerResponseStatus", "Server responses partitioned by status",
        "status",
        privacy_sandbox::server_common::metrics::kEmptyPublicPartition);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kRequestsSent("RequestsSent",
                  "Total number of requests sent to the server");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kEstimatedQPS("EstimatedQPS", "Estimated QPS");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kP50GrpcLatencyMs("P50GrpcLatency", "P50 Grpc request latency",
                      privacy_sandbox::server_common::metrics::kTimeHistogram);
inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kP90GrpcLatencyMs("P90GrpcLatency", "P50 Grpc request latency",
                      privacy_sandbox::server_common::metrics::kTimeHistogram);
inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kP99GrpcLatencyMs("P99GrpcLatency", "P50 Grpc request latency",
                      privacy_sandbox::server_common::metrics::kTimeHistogram);

inline constexpr const privacy_sandbox::server_common::metrics::DefinitionName*
    kRequestSimulationMetricsList[] = {
        &kRequestsSent,     &kServerResponseStatus, &kEstimatedQPS,
        &kP50GrpcLatencyMs, &kP90GrpcLatencyMs,     &kP99GrpcLatencyMs};
inline constexpr absl::Span<
    const privacy_sandbox::server_common::metrics::DefinitionName* const>
    kRequestSimulationMetricsSpan = kRequestSimulationMetricsList;

inline auto* RequestSimulationContextMap(
    std::optional<
        privacy_sandbox::server_common::telemetry::BuildDependentConfig>
        config = std::nullopt,
    std::unique_ptr<opentelemetry::metrics::MeterProvider> provider = nullptr,
    absl::string_view service = "Request-simulation",
    absl::string_view version = "") {
  return privacy_sandbox::server_common::metrics::GetContextMap<
      const std::string, kRequestSimulationMetricsSpan>(
      std::move(config), std::move(provider), service, version, {});
}
// MetricsCollector periodically collects metrics
// periodically prints metrics to the log and publishes metrics to
// Otel
class MetricsCollector {
 public:
  explicit MetricsCollector(std::unique_ptr<SleepFor> sleep_for);
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
  std::unique_ptr<ThreadManager> report_thread_manager_;
  std::unique_ptr<SleepFor> sleep_for_;
  grpc_histogram* histogram_per_interval_ ABSL_GUARDED_BY(mutex_);
  friend class MetricsCollectorPeer;
};

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_METRICS_COLLECTOR_H_
