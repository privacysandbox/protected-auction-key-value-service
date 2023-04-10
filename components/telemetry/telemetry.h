/*
 * Copyright 2022 Google LLC
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

#ifndef COMPONENTS_TELEMETRY_TELEMETRY_H_
#define COMPONENTS_TELEMETRY_TELEMETRY_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/trace/tracer.h"

namespace kv_server {

// Must be called to initialize telemetry functionality.
void InitTelemetry(std::string service_name, std::string build_version);

// Must be called to initialize metrics functionality.
// If `ConfigureMetrics` is not called, all metrics recording will be NoOp.
void ConfigureMetrics(
    opentelemetry::sdk::resource::Resource resource,
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions&
        options);

// Must be called to initialize tracing functionality.
// If `ConfigureTracer` is not called, all tracing will be NoOp.
void ConfigureTracer(opentelemetry::sdk::resource::Resource resource);

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer();

}  // namespace kv_server

#endif  // COMPONENTS_TELEMETRY_TELEMETRY_H_
