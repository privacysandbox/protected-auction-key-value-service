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
#include "glog/logging.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/trace/tracer.h"

namespace metric_sdk = opentelemetry::sdk::metrics;
using opentelemetry::sdk::metrics::MeterSelector;

namespace kv_server {
namespace {

// Workaround for no `constexpr opentelemetry::nostd::string_view`
constexpr char kSchema[] = "https://opentelemetry.io/schemas/1.2.0";
constexpr size_t kSchemaLen = sizeof(kSchema) - 1;
// The units below are nanoseconds.
constexpr double kDefaultHistogramBuckets[] = {
    40'000,        80'000,      120'000,     160'000,       220'000,
    280'000,       320'000,     640'000,     1'200'000,     2'500'000,
    5'000'000,     10'000'000,  20'000'000,  40'000'000,    80'000'000,
    160'000'000,   320'000'000, 640'000'000, 1'300'000'000, 2'600'000'000,
    5'000'000'000,
};

constexpr std::string_view kInstanceId = "instance_id";

class MetricsRecorderImpl : public MetricsRecorder {
 public:
  MetricsRecorderImpl(std::string service_name, std::string build_version)
      : service_name_(std::move(service_name)),
        build_version_(std::move(build_version)) {
    auto meter = GetMeter();
    event_count_ =
        meter->CreateUInt64Counter("EventCount", "Count of named events.");
    event_status_count_ = meter->CreateUInt64Counter(
        "EventStatus", "Count of status code associated with events.");

    // Catch all histogram, with prefconfigured buckets which should work for
    // most latencies distribution
    RegisterHistogramView("Latency", "Latency View", {});
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
    if (const auto key_iter = histograms_.find(event);
        key_iter != histograms_.end()) {
      return;
    }
    RegisterHistogramView(event, description, bucket_boundaries);
    auto meter = GetMeter();
    histograms_.insert_or_assign(
        event, meter->CreateUInt64Histogram(event, description, unit));
  }

 private:
  void RegisterHistogramView(std::string name, std::string description,
                             std::vector<double> bucket_boundaries) {
    if (bucket_boundaries.empty()) {
      bucket_boundaries.insert(bucket_boundaries.begin(),
                               std::begin(kDefaultHistogramBuckets),
                               std::end(kDefaultHistogramBuckets));
    }
    auto provider = opentelemetry::metrics::Provider::GetMeterProvider();
    // TODO: b/277098640 - Remove this dynamic cast.
    if (auto sdk_provider =
            dynamic_cast<metric_sdk::MeterProvider*>(provider.get());
        sdk_provider) {
      auto histogram_instrument_selector =
          std::make_unique<metric_sdk::InstrumentSelector>(
              metric_sdk::InstrumentType::kHistogram, name);
      auto histogram_meter_selector =
          GetMeterSelector(std::string({kSchema, kSchemaLen}));
      auto histogram_aggregation_config =
          std::make_shared<metric_sdk::HistogramAggregationConfig>();
      histogram_aggregation_config->boundaries_ = bucket_boundaries;
      auto histogram_view = std::make_unique<metric_sdk::View>(
          name, description, metric_sdk::AggregationType::kHistogram,
          std::move(histogram_aggregation_config));
      sdk_provider->AddView(std::move(histogram_instrument_selector),
                            std::move(histogram_meter_selector),
                            std::move(histogram_view));
    }
  }

  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> GetMeter()
      const {
    auto provider = opentelemetry::metrics::Provider::GetMeterProvider();
    return provider->GetMeter(service_name_, build_version_);
  }

  std::unique_ptr<MeterSelector> GetMeterSelector(std::string selector) const {
    return std::make_unique<MeterSelector>(service_name_, build_version_,
                                           selector);
  }

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
  std::string service_name_;
  std::string build_version_;
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

std::unique_ptr<MetricsRecorder> MetricsRecorder::Create(
    std::string service_name, std::string build_version) {
  return std::make_unique<MetricsRecorderImpl>(std::move(service_name),
                                               std::move(build_version));
}

}  // namespace kv_server
