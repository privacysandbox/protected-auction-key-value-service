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

#include "components/errors/retry.h"

#include <algorithm>

#include "absl/time/time.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::GetTracer;
using privacy_sandbox::server_common::TelemetryAttribute;
using privacy_sandbox::server_common::TraceWithStatus;

constexpr absl::Duration kMaxRetryInterval = absl::Minutes(2);
constexpr uint32_t kRetryBackoffBase = 2;
}  // namespace

absl::Duration ExponentialBackoffForRetry(uint32_t retries) {
  const absl::Duration backoff = absl::Seconds(pow(kRetryBackoffBase, retries));
  return std::min(backoff, kMaxRetryInterval);
}

void TraceRetryUntilOk(
    std::function<absl::Status()> func, std::string task_name,
    const absl::AnyInvocable<void(const absl::Status&, int) const>&
        metrics_callback,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  auto span = GetTracer()->StartSpan("RetryUntilOk - " + task_name);
  auto scope = opentelemetry::trace::Scope(span);
  auto wrapped = [func = std::move(func), task_name]() {
    return TraceWithStatus(std::move(func), task_name);
  };
  RetryUntilOk(std::move(wrapped), std::move(task_name), metrics_callback,
               log_context);
}

}  // namespace kv_server
