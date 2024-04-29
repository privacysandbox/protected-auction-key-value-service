/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_TOOLS_UTIL_CONFIGURE_TELEMETRY_TOOLS_H_
#define COMPONENTS_TOOLS_UTIL_CONFIGURE_TELEMETRY_TOOLS_H_
#include <memory>

#include "components/telemetry/server_definition.h"
#include "opentelemetry/exporters/ostream/log_record_exporter.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"
#include "src/logger/request_context_impl.h"

namespace kv_server {

// Configure telemetry and logger for tools
inline void ConfigureTelemetryForTools() {
  // Init noop telemetry for metrics
  InitMetricsContextMap();
  // Init logger to write to console
  static opentelemetry::logs::LoggerProvider* logger_provider =
      opentelemetry::sdk::logs::LoggerProviderFactory::Create(
          opentelemetry::sdk::logs::SimpleLogRecordProcessorFactory::Create(
              std::make_unique<
                  opentelemetry::exporter::logs::OStreamLogRecordExporter>(
                  std::cerr)))
          .release();
  privacy_sandbox::server_common::log::logger_private =
      logger_provider->GetLogger("default").get();
  // Turn on all logging
  privacy_sandbox::server_common::log::AlwaysLogOtel(true);
}

}  // namespace kv_server

#endif  // COMPONENTS_TOOLS_UTIL_CONFIGURE_TELEMETRY_TOOLS_H_
