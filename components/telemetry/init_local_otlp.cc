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

#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h"
#include "opentelemetry/sdk/trace/random_id_generator_factory.h"
#include "src/telemetry/init.h"

// To use Jaeger first run a local instance of the collector
// https://www.jaegertracing.io/docs/1.42/getting-started/
// Then build run server with flags for local and otlp.  Ex:
// `bazel run //components/data_server/server:server --config=local_instance
// --config=aws_platform
// --@google_privacysandbox_servers_common//src/telemetry:local_otel_export=otlp
// --
// --environment="test"`
namespace kv_server {

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> CreateSpanExporter() {
  return opentelemetry::exporter::otlp::OtlpGrpcExporterFactory::Create();
}

std::unique_ptr<opentelemetry::sdk::trace::IdGenerator> CreateIdGenerator() {
  return opentelemetry::sdk::trace::RandomIdGeneratorFactory::Create();
}

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>
CreatePeriodicExportingMetricReader(
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions&
        options) {
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter =
      opentelemetry::exporter::otlp::OtlpGrpcMetricExporterFactory::Create();
  return std::make_unique<
      opentelemetry::sdk::metrics::PeriodicExportingMetricReader>(
      std::move(exporter), options);
}

}  // namespace kv_server
