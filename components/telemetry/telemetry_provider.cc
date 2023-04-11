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

#include "telemetry_provider.h"

#include <memory>
#include <utility>

#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/trace/provider.h"

namespace nostd = opentelemetry::nostd;
using opentelemetry::metrics::Meter;
using opentelemetry::sdk::metrics::MeterSelector;
using opentelemetry::trace::Tracer;

namespace kv_server {

void TelemetryProvider::Init(std::string service_name,
                             std::string build_version) {
  auto& instance = TelemetryProvider::GetInstance();
  instance.service_name_ = std::move(service_name);
  instance.build_version_ = std::move(build_version);
}

TelemetryProvider& TelemetryProvider::GetInstance() {
  return *telemetry_provider_;
}

TelemetryProvider* TelemetryProvider::telemetry_provider_ =
    new TelemetryProvider();

nostd::shared_ptr<Tracer> TelemetryProvider::GetTracer() const {
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer(service_name_, build_version_);
}

}  // namespace kv_server
