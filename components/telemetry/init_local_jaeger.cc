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

#include "components/telemetry/init.h"
#include "opentelemetry/exporters/jaeger/jaeger_exporter_factory.h"
#include "opentelemetry/sdk/trace/random_id_generator_factory.h"

// To use Jaeger first run a local instance of the collector
// https://www.jaegertracing.io/docs/1.21/getting-started/
// Then build run server with flags for local and jaeger.  Ex:
// `bazel run //components/data_server/server:server --//:instance=local
// --//:platform=aws --//components/telemetry:local_otel_export=jaeger --
// --environment="test"`
namespace kv_server {

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> CreateSpanExporter() {
  return opentelemetry::exporter::jaeger::JaegerExporterFactory::Create();
}

std::unique_ptr<opentelemetry::sdk::trace::IdGenerator> CreateIdGenerator() {
  return opentelemetry::sdk::trace::RandomIdGeneratorFactory::Create();
}

}  // namespace kv_server