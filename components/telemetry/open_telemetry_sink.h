/*
 * Copyright 2024 Google LLC
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

#ifndef COMPONENTS_TELEMETRY_OPEN_TELEMETRY_SINK_H_
#define COMPONENTS_TELEMETRY_OPEN_TELEMETRY_SINK_H_

#include <memory>

#include "absl/log/log.h"
#include "src/telemetry/telemetry.h"

namespace kv_server {

class OpenTelemetrySink : public absl::LogSink {
 public:
  explicit OpenTelemetrySink(
      opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger> logger);
  void Send(const absl::LogEntry& entry) override;
  void Flush() override;
  opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger> logger_;
};

}  // namespace kv_server

#endif  // COMPONENTS_TELEMETRY_OPEN_TELEMETRY_SINK_H_
