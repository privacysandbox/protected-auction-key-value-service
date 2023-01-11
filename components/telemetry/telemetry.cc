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
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/sdk/trace/samplers/always_on_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/sdk/version/version.h"
#include "opentelemetry/trace/provider.h"

namespace kv_server {

// Workaround for no `constexpr opentelemetry::nostd::string_view`
constexpr char kTracerLibName[] = "KVServer";
constexpr size_t kNameLen = sizeof(kTracerLibName) - 1;

void InitTracer() {
  auto exporter = CreateSpanExporter();
  auto processor =
      opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(
          std::move(exporter));
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      opentelemetry::sdk::trace::TracerProviderFactory::Create(
          std::move(processor),
          opentelemetry::sdk::resource::Resource::Create({}),
          opentelemetry::sdk::trace::AlwaysOnSamplerFactory::Create(),
          CreateIdGenerator());

  // Set the global trace provider
  opentelemetry::trace::Provider::SetTracerProvider(provider);
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer() {
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer({kTracerLibName, kNameLen}, BuildVersion().data());
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
