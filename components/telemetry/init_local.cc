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

#include "opentelemetry/exporters/ostream/metric_exporter.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"

namespace kv_server {

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>
CreatePeriodicExportingMetricReader(
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions&
        options) {
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter =
      std::make_unique<
          opentelemetry::exporter::metrics::OStreamMetricExporter>();
  return std::make_unique<
      opentelemetry::sdk::metrics::PeriodicExportingMetricReader>(
      std::move(exporter), options);
}

}  // namespace kv_server
