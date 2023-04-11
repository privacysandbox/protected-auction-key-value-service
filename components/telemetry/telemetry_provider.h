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

#ifndef COMPONENTS_TELEMETRY_TELEMETRY_PROVIDER_H_
#define COMPONENTS_TELEMETRY_TELEMETRY_PROVIDER_H_

#include <memory>
#include <string>

#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/trace/tracer.h"

#include "metrics_recorder.h"

namespace kv_server {

class TelemetryProvider {
 public:
  static void Init(std::string service_name, std::string build_version);

  static TelemetryProvider& GetInstance();

  // Creates a new metrics recorder
  std::unique_ptr<MetricsRecorder> CreateMetricsRecorder() {
    return MetricsRecorder::Create(service_name_, build_version_);
  }

  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer()
      const;

  TelemetryProvider(TelemetryProvider& provider) = delete;
  void operator=(const TelemetryProvider&) = delete;

 private:
  TelemetryProvider()
      : service_name_("uninitialized"), build_version_("uninitialized") {}

  static TelemetryProvider* telemetry_provider_;
  std::string service_name_;
  std::string build_version_;
};

}  // namespace kv_server
#endif  // COMPONENTS_TELEMETRY_TELEMETRY_PROVIDER_H_
