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

#include "absl/container/flat_hash_map.h"
#include "components/telemetry/telemetry.h"
#include "components/util/build_info.h"

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

  void IncrementEventCounter(std::string name) override {
    absl::flat_hash_map<std::string, std::string> labels = {
        {"event", std::move(name)}};
    const auto labelkv =
        opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
    event_count_->Add(1, labelkv);
  }

 private:
  opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>>
      event_status_count_;
  opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>>
      event_count_;
};

class MetricsRecorderBlankImpl : public MetricsRecorder {
 public:
  void IncrementEventStatus(std::string event, absl::Status status,
                            uint64_t count = 1) override {}

  void IncrementEventCounter(std::string name) override {}
};

}  // namespace

std::unique_ptr<MetricsRecorder> MetricsRecorder::Create() {
  return std::make_unique<MetricsRecorderImpl>();
}

std::unique_ptr<MetricsRecorder> MetricsRecorder::CreateBlank() {
  return std::make_unique<MetricsRecorderBlankImpl>();
}

}  // namespace kv_server
