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

#ifndef COMPONENTS_TELEMETRY_TRACING_H_
#define COMPONENTS_TELEMETRY_TRACING_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "opentelemetry/sdk/trace/tracer.h"

#include "telemetry.h"

namespace kv_server {

// Translates the `absl::Status` to `opentelemetry::StatusCode`.
// If the status is not `ok`, the string representation of `status` is used.
void SetStatus(const absl::Status& status, opentelemetry::trace::Span& span);

// Key value pair of an attribute. (ex: 'filename': 'DELTA_123')
struct TelemetryAttribute {
  std::string label;
  opentelemetry::common::AttributeValue value;
};

// Traces `func` associating with `name`.
// Optionally a vector of `attributes` can be added to the trace of the
// function.
absl::Status TraceWithStatus(std::function<absl::Status()> func,
                             opentelemetry::nostd::string_view name,
                             std::vector<TelemetryAttribute> attributes = {});

template <typename Func>
typename std::invoke_result_t<Func> TraceWithStatusOr(
    Func&& func, opentelemetry::nostd::string_view name,
    std::vector<TelemetryAttribute> attributes = {}) {
  auto span = GetTracer()->StartSpan(name);
  auto scope = opentelemetry::trace::Scope(span);
  for (const auto& attribute : attributes) {
    span->SetAttribute(attribute.label, attribute.value);
  }
  auto statusor = func();
  SetStatus(statusor.status(), *span);
  return statusor;
}

}  // namespace kv_server

#endif  // COMPONENTS_TELEMETRY_TRACING_H_
