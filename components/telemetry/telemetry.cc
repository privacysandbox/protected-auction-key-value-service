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
#include "components/util/build_info.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/view/view_registry.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"
#include "opentelemetry/sdk/trace/samplers/always_on_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/sdk/version/version.h"
#include "opentelemetry/trace/provider.h"

namespace metric_sdk = opentelemetry::sdk::metrics;
namespace metrics_api = opentelemetry::metrics;
namespace nostd = opentelemetry::nostd;
namespace trace = opentelemetry::trace;
namespace semantic_conventions =
    opentelemetry::sdk::resource::SemanticConventions;
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

namespace {
// Workaround for no `constexpr opentelemetry::nostd::string_view`
constexpr char kTracerLibName[] = "KVServer";
constexpr size_t kNameLen = sizeof(kTracerLibName) - 1;
constexpr char kSchema[] = "https://opentelemetry.io/schemas/1.2.0";
constexpr size_t kSchemaLen = sizeof(kSchema) - 1;
// The units below are nanoseconds.
const std::vector<double> kDefaultHistogramBuckets = {
    40'000,        80'000,      120'000,     160'000,       220'000,
    280'000,       320'000,     640'000,     1'200'000,     2'500'000,
    5'000'000,     10'000'000,  20'000'000,  40'000'000,    80'000'000,
    160'000'000,   320'000'000, 640'000'000, 1'300'000'000, 2'600'000'000,
    5'000'000'000,
};
static bool metrics_initialized = false;

ResourceAttributes CreateCommonAttributes(std::string instance_id) {
  return ResourceAttributes{
      {semantic_conventions::kServiceName, "kv-server"},
      {semantic_conventions::kServiceVersion, std::string(BuildVersion())},
      {semantic_conventions::kHostArch, std::string(BuildPlatform())},
      {semantic_conventions::kServiceInstanceId, std::move(instance_id)}};
}

}  // namespace

void InitTracer(std::string instance_id) {
  auto exporter = CreateSpanExporter();
  auto processor = SimpleSpanProcessorFactory::Create(std::move(exporter));
  const auto resource =
      Resource::Create(CreateCommonAttributes(std::move(instance_id)));
  std::shared_ptr<TracerProvider> provider = TracerProviderFactory::Create(
      std::move(processor), resource, AlwaysOnSamplerFactory::Create(),
      CreateIdGenerator());

  // Set the global trace provider
  Provider::SetTracerProvider(provider);
}

void RegisterHistogramView(std::string name, std::string description,
                           std::vector<double> bucket_boundaries) {
  if (!metrics_initialized) {
    return;
  }
  if (bucket_boundaries.empty()) {
    bucket_boundaries = kDefaultHistogramBuckets;
  }
  auto provider = metrics_api::Provider::GetMeterProvider();
  auto p = static_cast<metric_sdk::MeterProvider*>(provider.get());
  auto histogram_instrument_selector =
      std::make_unique<metric_sdk::InstrumentSelector>(
          metric_sdk::InstrumentType::kHistogram, name);
  auto histogram_meter_selector = std::make_unique<metric_sdk::MeterSelector>(
      std::string({kTracerLibName, kNameLen}), BuildVersion().data(),
      std::string({kSchema, kSchemaLen}));
  auto histogram_aggregation_config =
      std::make_shared<metric_sdk::HistogramAggregationConfig>();
  histogram_aggregation_config->boundaries_ = bucket_boundaries;
  auto histogram_view = std::make_unique<metric_sdk::View>(
      name, description, metric_sdk::AggregationType::kHistogram,
      std::move(histogram_aggregation_config));
  p->AddView(std::move(histogram_instrument_selector),
             std::move(histogram_meter_selector), std::move(histogram_view));
}

void InitMetrics(
    std::string instance_id,
    const metric_sdk::PeriodicExportingMetricReaderOptions& options) {
  auto reader = CreatePeriodicExportingMetricReader(options);
  auto resource =
      Resource::Create(CreateCommonAttributes(std::move(instance_id)));
  std::shared_ptr<metrics_api::MeterProvider> provider =
      std::make_shared<metric_sdk::MeterProvider>(
          std::make_unique<metric_sdk::ViewRegistry>(), std::move(resource));
  std::shared_ptr<metric_sdk::MeterProvider> p =
      std::static_pointer_cast<metric_sdk::MeterProvider>(provider);
  p->AddMetricReader(std::move(reader));
  metrics_api::Provider::SetMeterProvider(provider);
  metrics_initialized = true;
}

nostd::shared_ptr<Tracer> GetTracer() {
  auto provider = Provider::GetTracerProvider();
  return provider->GetTracer({kTracerLibName, kNameLen}, BuildVersion().data());
}

nostd::shared_ptr<metrics_api::Meter> GetMeter() {
  auto provider = metrics_api::Provider::GetMeterProvider();
  return provider->GetMeter({kTracerLibName, kNameLen}, BuildVersion().data());
}

void SetStatus(const absl::Status& status, Span& span) {
  if (status.ok()) {
    span.SetStatus(StatusCode::kOk);
  } else {
    span.SetStatus(StatusCode::kError, status.ToString());
  }
}

absl::Status TraceWithStatus(std::function<absl::Status()> func,
                             nostd::string_view name,
                             std::vector<TelemetryAttribute> attributes) {
  auto span = GetTracer()->StartSpan(name);
  auto scope = Scope(span);
  for (const auto& attribute : attributes) {
    span->SetAttribute(attribute.key, attribute.value);
  }
  const auto status = func();
  SetStatus(status, *span);
  return status;
}

}  // namespace kv_server
