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

#include "glog/logging.h"

ABSL_FLAG(absl::Duration, metrics_report_interval, absl::Minutes(1),
          "The interval for reporting metrics");

namespace kv_server {

constexpr char* kP50GrpcLatency = "P50GrpcLatency";
constexpr char* kP90GrpcLatency = "P90GrpcLatency";
constexpr char* kP99GrpcLatency = "P99GrpcLatency";
constexpr char* kEstimatedQPS = "EstimatedQPS";
constexpr char* kRequestsSent = "RequestsSent";
constexpr char* KServerResponseStatus = "ServerResponseStatus";

constexpr double kDefaultHistogramResolution = 0.1;
constexpr double kDefaultHistogramMaxBucket = 60e9;

MetricsCollector::MetricsCollector(
    privacy_sandbox::server_common::MetricsRecorder& metrics_recorder,
    std::unique_ptr<SleepFor> sleep_for)
    : requests_sent_per_interval_(0),
      requests_with_ok_response_per_interval_(0),
      requests_with_error_response_per_interval_(0),
      report_interval_(std::move(absl::GetFlag(FLAGS_metrics_report_interval))),
      report_thread_manager_(
          TheadManager::Create("Metrics periodic report thread")),
      metrics_recorder_(metrics_recorder),
      sleep_for_(std::move(sleep_for)) {
  histogram_per_interval_ = grpc_histogram_create(kDefaultHistogramResolution,
                                                  kDefaultHistogramMaxBucket);
  metrics_recorder_.RegisterHistogram(kRequestsSent, "Requests sent", "");
  metrics_recorder_.RegisterHistogram(kEstimatedQPS, "Estimated QPS", "");
  metrics_recorder_.RegisterHistogram(kP50GrpcLatency, "P50 Latency",
                                      "microsecond");
  metrics_recorder_.RegisterHistogram(kP90GrpcLatency, "P90 Latency",
                                      "microsecond");
  metrics_recorder_.RegisterHistogram(kP99GrpcLatency, "P99 Latency",
                                      "microsecond");
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
    metrics_recorder_.RecordHistogramEvent(kRequestsSent, requests_sent);
    metrics_recorder_.RecordHistogramEvent(kEstimatedQPS, estimated_qps);
    metrics_recorder_.RecordHistogramEvent(
        kP50GrpcLatency, absl::ToInt64Microseconds(p50_latency));
    metrics_recorder_.RecordHistogramEvent(
        kP90GrpcLatency, absl::ToInt64Microseconds(p90_latency));
    metrics_recorder_.RecordHistogramEvent(
        kP99GrpcLatency, absl::ToInt64Microseconds(p99_latency));
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
  metrics_recorder_.IncrementEventStatus(KServerResponseStatus, status);
}

}  // namespace kv_server
