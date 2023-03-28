// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/telemetry/metrics_recorder.h"

#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "components/telemetry/telemetry.h"
#include "components/util/build_info.h"
#include "glog/logging.h"

namespace kv_server {
namespace {

constexpr std::string_view kInstanceId = "instance_id";

class MetricsRecorderImpl : public MetricsRecorder {
 public:
  MetricsRecorderImpl() {
    auto meter = GetMeter();
    event_count_ =
        meter->CreateUInt64Counter("EventCount", "Count of named events.");
    event_status_count_ = meter->CreateUInt64Counter(
        "EventStatus", "Count of status code associated with events.");

    // Catch all histogram, with prefconfigured buckets which should work for
    // most latencies distribution
    RegisterHistogramView("Latency", "Latency View");
    latency_histogram_ = meter->CreateUInt64Histogram(
        "Latency", "Histogram of latencies associated with events.",
        "nanosecond");
  }

  void IncrementEventStatus(std::string event, absl::Status status,
                            uint64_t count = 1) override {
    absl::flat_hash_map<std::string, std::string> labels = {
        {"event", std::move(event)},
        {"status", absl::StatusCodeToString(status.code())}};
    const auto labelkv =
        opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
    event_status_count_->Add(count, labelkv);
  }

  void RecordHistogramEvent(std::string event, int64_t value) override {
    absl::flat_hash_map<std::string, std::string> labels = {{"event", event}};
    const auto labelkv =
        opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
    auto context = opentelemetry::context::RuntimeContext::GetCurrent();
    absl::ReaderMutexLock lock(&mutex_);
    const auto key_iter = histograms_.find(event);

    if (key_iter != histograms_.end()) {
      key_iter->second->Record(value, labelkv, context);
    } else {
      LOG(ERROR) << "The following histogram hasn't been initialized: "
                 << event;
    }
  }

  void RecordLatency(std::string event, absl::Duration duration) override {
    absl::flat_hash_map<std::string, std::string> labels = {
        {"event", std::move(event)}};
    const auto labelkv =
        opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
    auto context = opentelemetry::context::RuntimeContext::GetCurrent();
    latency_histogram_->Record(absl::ToInt64Nanoseconds(duration), labelkv,
                               context);
  }

  void IncrementEventCounter(std::string event) override {
    absl::flat_hash_map<std::string, std::string> labels = {
        {"event", std::move(event)}};
    const auto labelkv =
        opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
    event_count_->Add(1, labelkv);
  }

  void RegisterHistogram(std::string event, std::string description,
                         std::string unit,
                         std::vector<double> bucket_boundaries) override {
    absl::MutexLock lock(&mutex_);
    const auto key_iter = histograms_.find(event);
    if (key_iter != histograms_.end()) {
      return;
    }

    RegisterHistogramView(event, description, bucket_boundaries);
    auto meter = GetMeter();
    histograms_.insert_or_assign(
        event, meter->CreateUInt64Histogram(event, description, unit));
  }

 private:
  opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>>
      event_status_count_;
  opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<uint64_t>>
      latency_histogram_;
  opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>>
      event_count_;
  mutable absl::Mutex mutex_;
  absl::flat_hash_map<std::string,
                      opentelemetry::nostd::unique_ptr<
                          opentelemetry::metrics::Histogram<uint64_t>>>
      histograms_ ABSL_GUARDED_BY(mutex_);
};

class NoOpMetricsRecorderImpl : public MetricsRecorder {
 public:
  void IncrementEventStatus(std::string event, absl::Status status,
                            uint64_t count = 1) override {}

  void IncrementEventCounter(std::string event) override {}
  void RecordLatency(std::string event, absl::Duration duration) override {}
  void RecordHistogramEvent(std::string event, int64_t value) override {}
  void RegisterHistogram(std::string event, std::string description,
                         std::string unit,
                         std::vector<double> bucket_boundaries) override {}
};

}  // namespace

ScopeLatencyRecorder::ScopeLatencyRecorder(std::string event_name,
                                           MetricsRecorder& metrics_recorder)
    : stop_watch_(Stopwatch()),
      event_name_(std::move(event_name)),
      metrics_recorder_(metrics_recorder) {}
ScopeLatencyRecorder::~ScopeLatencyRecorder() {
  metrics_recorder_.RecordLatency(event_name_, GetLatency());
}
absl::Duration ScopeLatencyRecorder::GetLatency() {
  return stop_watch_.GetElapsedTime();
}

MetricsRecorder& MetricsRecorder::GetInstance() {
  static auto* const instance = new MetricsRecorderImpl();
  return *instance;
}

MetricsRecorder& MetricsRecorder::GetNoOpInstance() {
  static auto* const instance = new NoOpMetricsRecorderImpl();
  return *instance;
}

}  // namespace kv_server
