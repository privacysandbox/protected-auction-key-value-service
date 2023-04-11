// Copyright 2022 Google LLC
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

#include "components/telemetry/telemetry.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/telemetry/init.h"
#include "components/telemetry/telemetry_provider.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/view/view_registry.h"
#include "opentelemetry/sdk/trace/samplers/always_on_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/sdk/version/version.h"
#include "opentelemetry/trace/provider.h"

namespace metric_sdk = opentelemetry::sdk::metrics;
namespace metrics_api = opentelemetry::metrics;
namespace nostd = opentelemetry::nostd;
namespace trace = opentelemetry::trace;
using opentelemetry::sdk::resource::Resource;
using opentelemetry::sdk::resource::ResourceAttributes;
using opentelemetry::sdk::trace::AlwaysOnSamplerFactory;
using opentelemetry::sdk::trace::SimpleSpanProcessorFactory;
using opentelemetry::sdk::trace::TracerProviderFactory;
using opentelemetry::trace::Provider;
using opentelemetry::trace::Scope;
using opentelemetry::trace::Span;
using opentelemetry::trace::StatusCode;
using opentelemetry::trace::Tracer;
using opentelemetry::trace::TracerProvider;

namespace kv_server {

void InitTelemetry(std::string service_name, std::string build_version) {
  TelemetryProvider::Init(service_name, build_version);
}

void ConfigureMetrics(
    Resource resource,
    const metric_sdk::PeriodicExportingMetricReaderOptions& options) {
  auto reader = CreatePeriodicExportingMetricReader(options);
  std::shared_ptr<metrics_api::MeterProvider> provider =
      std::make_shared<metric_sdk::MeterProvider>(
          std::make_unique<metric_sdk::ViewRegistry>(), std::move(resource));
  std::shared_ptr<metric_sdk::MeterProvider> p =
      std::static_pointer_cast<metric_sdk::MeterProvider>(provider);
  p->AddMetricReader(std::move(reader));
  metrics_api::Provider::SetMeterProvider(provider);
}

void ConfigureTracer(Resource resource) {
  auto exporter = CreateSpanExporter();
  auto processor = SimpleSpanProcessorFactory::Create(std::move(exporter));
  std::shared_ptr<TracerProvider> provider = TracerProviderFactory::Create(
      std::move(processor), resource, AlwaysOnSamplerFactory::Create(),
      CreateIdGenerator());

  // Set the global trace provider
  Provider::SetTracerProvider(provider);
}

nostd::shared_ptr<Tracer> GetTracer() {
  return TelemetryProvider::GetInstance().GetTracer();
}

}  // namespace kv_server
