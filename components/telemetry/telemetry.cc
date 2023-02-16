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

namespace kv_server {

namespace {
// Workaround for no `constexpr opentelemetry::nostd::string_view`
constexpr char kTracerLibName[] = "KVServer";
constexpr size_t kNameLen = sizeof(kTracerLibName) - 1;

opentelemetry::sdk::resource::ResourceAttributes CreateCommonAttributes(
    std::string instance_id) {
  return opentelemetry::sdk::resource::ResourceAttributes{
      {opentelemetry::sdk::resource::SemanticConventions::kServiceName,
       "kv-server"},
      {opentelemetry::sdk::resource::SemanticConventions::kServiceVersion,
       std::string(BuildVersion())},
      {opentelemetry::sdk::resource::SemanticConventions::kHostArch,
       std::string(BuildPlatform())},
      {opentelemetry::sdk::resource::SemanticConventions::kServiceInstanceId,
       std::move(instance_id)}};
}

}  // namespace

void InitTracer(std::string instance_id) {
  auto exporter = CreateSpanExporter();
  auto processor =
      opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(
          std::move(exporter));
  const auto resource = opentelemetry::sdk::resource::Resource::Create(
      CreateCommonAttributes(std::move(instance_id)));
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      opentelemetry::sdk::trace::TracerProviderFactory::Create(
          std::move(processor), resource,
          opentelemetry::sdk::trace::AlwaysOnSamplerFactory::Create(),
          CreateIdGenerator());

  // Set the global trace provider
  opentelemetry::trace::Provider::SetTracerProvider(provider);
}

void InitMetrics(
    std::string instance_id,
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions&
        options) {
  auto reader = CreatePeriodicExportingMetricReader(options);
  auto resource = opentelemetry::sdk::resource::Resource::Create(
      CreateCommonAttributes(std::move(instance_id)));
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider =
      std::make_shared<opentelemetry::sdk::metrics::MeterProvider>(
          std::make_unique<opentelemetry::sdk::metrics::ViewRegistry>(),
          std::move(resource));
  auto p = std::static_pointer_cast<opentelemetry::sdk::metrics::MeterProvider>(
      provider);
  p->AddMetricReader(std::move(reader));
  opentelemetry::metrics::Provider::SetMeterProvider(provider);
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer() {
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer({kTracerLibName, kNameLen}, BuildVersion().data());
}

opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> GetMeter() {
  auto provider = opentelemetry::metrics::Provider::GetMeterProvider();
  return provider->GetMeter({kTracerLibName, kNameLen}, BuildVersion().data());
}

void SetStatus(const absl::Status& status, opentelemetry::trace::Span& span) {
  if (status.ok()) {
    span.SetStatus(opentelemetry::trace::StatusCode::kOk);
  } else {
    span.SetStatus(opentelemetry::trace::StatusCode::kError, status.ToString());
  }
}

absl::Status TraceWithStatus(std::function<absl::Status()> func,
                             opentelemetry::nostd::string_view name,
                             std::vector<TelemetryAttribute> attributes) {
  auto span = GetTracer()->StartSpan(name);
  auto scope = opentelemetry::trace::Scope(span);
  for (const auto& attribute : attributes) {
    span->SetAttribute(attribute.key, attribute.value);
  }
  const auto status = func();
  SetStatus(status, *span);
  return status;
}

}  // namespace kv_server
