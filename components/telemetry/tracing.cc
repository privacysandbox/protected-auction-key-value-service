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

#include "tracing.h"

#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "opentelemetry/sdk/trace/tracer.h"

namespace nostd = opentelemetry::nostd;
using opentelemetry::trace::Scope;
using opentelemetry::trace::Span;
using opentelemetry::trace::StatusCode;

namespace kv_server {

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
    span->SetAttribute(attribute.label, attribute.value);
  }
  const auto status = func();
  SetStatus(status, *span);
  return status;
}

}  // namespace kv_server
