// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/log/initialize.h"
#include "components/data_server/server/server_log_init.h"
#include "src/logger/request_context_logger.h"

namespace kv_server {

void InitLog() {
  absl::InitializeLog();
  // Turn on all otel logging for nonprod build regardless of consented or not
  // This line must be called before the first PS_LOG/PS_VLOG line.
  privacy_sandbox::server_common::log::AlwaysLogOtel(true);
}

}  // namespace kv_server
