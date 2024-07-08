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

#include <utility>

#include "absl/log/log.h"

ABSL_FLAG(absl::Duration, metrics_report_interval, absl::Minutes(1),
          "The interval for reporting metrics");

namespace kv_server {

namespace {
constexpr double kDefaultHistogramResolution = 0.1;
constexpr double kDefaultHistogramMaxBucket = 60e9;
}  // namespace

MetricsCollector::MetricsCollector(std::unique_ptr<SleepFor> sleep_for)
    : requests_sent_per_interval_(0),
      requests_with_ok_response_per_interval_(0),
      requests_with_error_response_per_interval_(0),
      report_interval_(std::move(absl::GetFlag(FLAGS_metrics_report_interval))),
      report_thread_manager_(
          ThreadManager::Create("Metrics periodic report thread")),
      sleep_for_(std::move(sleep_for)) {
  histogram_per_interval_ = grpc_histogram_create(kDefaultHistogramResolution,
                                                  kDefaultHistogramMaxBucket);
}

void MetricsCollector::AddLatencyToHistogram(absl::Duration latency) {
  absl::MutexLock lock(&mutex_);
  grpc_histogram_add(histogram_per_interval_,
                     absl::ToDoubleMicroseconds(latency));
}
absl::Duration MetricsCollector::GetPercentileLatency(double percentile) {
  absl::MutexLock lock(&mutex_);
  return absl::Microseconds(
      grpc_histogram_percentile(histogram_per_interval_, percentile));
}

void MetricsCollector::IncrementRequestsWithOkResponsePerInterval() {
  requests_with_ok_response_per_interval_.fetch_add(1,
                                                    std::memory_order_relaxed);
}

void MetricsCollector::IncrementRequestsWithErrorResponsePerInterval() {
  requests_with_error_response_per_interval_.fetch_add(
      1, std::memory_order_relaxed);
}
void MetricsCollector::IncrementRequestSentPerInterval() {
  requests_sent_per_interval_.fetch_add(1, std::memory_order_relaxed);
}
absl::Status MetricsCollector::Start() {
  return report_thread_manager_->Start([this]() { PublishMetrics(); });
}
absl::Status MetricsCollector::Stop() { return report_thread_manager_->Stop(); }

void MetricsCollector::PublishMetrics() {
  while (!report_thread_manager_->ShouldStop()) {
    sleep_for_->Duration(report_interval_);
    auto requests_sent =
        requests_with_ok_response_per_interval_.load(std::memory_order_relaxed);
    auto requests_with_ok_responses =
        requests_with_ok_response_per_interval_.load(std::memory_order_relaxed);
    auto requests_with_error_responses =
        requests_with_error_response_per_interval_.load(
            std::memory_order_relaxed);
    auto estimated_qps = GetQPS();
    auto p50_latency = GetPercentileLatency(0.5);
    auto p90_latency = GetPercentileLatency(0.9);
    auto p99_latency = GetPercentileLatency(0.99);
    RequestSimulationContextMap()->SafeMetric().LogUpDownCounter<kRequestsSent>(
        (int)requests_sent);
    RequestSimulationContextMap()->SafeMetric().LogUpDownCounter<kEstimatedQPS>(
        (int)estimated_qps);
    RequestSimulationContextMap()->SafeMetric().LogHistogram<kP99GrpcLatencyMs>(
        ((int)absl::ToInt64Milliseconds(p99_latency)));
    RequestSimulationContextMap()->SafeMetric().LogHistogram<kP90GrpcLatencyMs>(
        ((int)absl::ToInt64Milliseconds(p90_latency)));
    RequestSimulationContextMap()->SafeMetric().LogHistogram<kP50GrpcLatencyMs>(
        ((int)absl::ToInt64Milliseconds(p50_latency)));
    LOG(INFO) << "Metrics Summary: ";
    LOG(INFO) << "Number of requests sent:" << requests_sent;
    LOG(INFO) << "Number of requests with ok responses:"
              << requests_with_ok_responses;
    LOG(INFO) << "Number of requests with error responses:"
              << requests_with_error_responses;
    LOG(INFO) << "Estimated QPS " << estimated_qps;
    LOG(INFO) << "P50 latency " << p50_latency;
    LOG(INFO) << "P90 latency " << p90_latency;
    LOG(INFO) << "P99 latency " << p99_latency;
    ResetHistogram();
    ResetRequestsPerInterval();
  }
}
void MetricsCollector::ResetRequestsPerInterval() {
  requests_sent_per_interval_ = 0;
  requests_with_ok_response_per_interval_ = 0;
  requests_with_error_response_per_interval_ = 0;
}

void MetricsCollector::ResetHistogram() {
  absl::MutexLock lock(&mutex_);
  if (histogram_per_interval_) {
    grpc_histogram_destroy(histogram_per_interval_);
  }
  histogram_per_interval_ = grpc_histogram_create(kDefaultHistogramResolution,
                                                  kDefaultHistogramMaxBucket);
}

int64_t MetricsCollector::GetQPS() {
  return requests_with_ok_response_per_interval_.load(
             std::memory_order_relaxed) /
         absl::ToInt64Seconds(report_interval_);
}
void MetricsCollector::IncrementServerResponseStatusEvent(
    const absl::Status& status) {
  RequestSimulationContextMap()
      ->SafeMetric()
      .LogUpDownCounter<kServerResponseStatus>(
          {{absl::StatusCodeToString(status.code()), 1}});
}

}  // namespace kv_server
