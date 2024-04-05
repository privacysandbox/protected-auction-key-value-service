// Copyright 2024 Google LLC
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

#include "components/telemetry/open_telemetry_sink.h"

#include <utility>

namespace kv_server {
OpenTelemetrySink::OpenTelemetrySink(
    opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger> logger)
    : logger_(std::move(logger)) {}
void OpenTelemetrySink::Send(const absl::LogEntry& entry) {
  logger_->EmitLogRecord(entry.text_message_with_prefix_and_newline_c_str());
}
// opentelemetry::logs::Logger doesn't have flush
void OpenTelemetrySink::Flush() {}
}  // namespace kv_server
