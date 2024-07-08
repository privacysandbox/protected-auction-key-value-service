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

#ifndef COMPONENTS_ERRORS_RETRY_H_
#define COMPONENTS_ERRORS_RETRY_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "components/telemetry/server_definition.h"
#include "components/util/sleepfor.h"
#include "src/logger/request_context_logger.h"
#include "src/telemetry/tracing.h"

namespace kv_server {

// Retry the function with exponential backoff until it succeeds.
absl::Duration ExponentialBackoffForRetry(uint32_t retries);

// You shouldn't need to instantiate this class.
// Use `RetryWithMax/RetryUntilOk` which creates one for you.
template <typename Func>
class RetryableWithMax {
 public:
  // Special retry value to denote unlimited retries. Made public for better
  // documentation purposes at call sites.
  static constexpr int kUnlimitedRetry = -1;

  // If max_attempts <= 0, will retry until OK.
  // `metrics_callback` is optional.
  RetryableWithMax(
      Func&& f, std::string task_name, int max_attempts,
      const absl::AnyInvocable<void(const absl::Status&, int) const>&
          metrics_callback,
      const SleepFor& sleep_for,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : func_(std::forward<Func>(f)),
        task_name_(std::move(task_name)),
        max_attempts_(max_attempts <= 0 ? kUnlimitedRetry : max_attempts),
        metrics_callback_(metrics_callback),
        sleep_for_(sleep_for),
        log_context_(log_context) {}

  absl::Status ToStatus(absl::Status& result) { return result; }

  template <typename = typename std::enable_if_t<
                !std::is_same<std::invoke_result<Func>, absl::Status>::value>>
  absl::Status ToStatus(std::invoke_result_t<Func>& result) {
    return result.status();
  }

  typename std::invoke_result_t<Func> operator()() {
    std::invoke_result_t<Func> result;
    for (int i = 1; max_attempts_ == kUnlimitedRetry || i <= max_attempts_;
         ++i) {
      result = func_();
      metrics_callback_(ToStatus(result), 1);
      if (result.ok()) {
        return result;
      } else {
        PS_LOG(WARNING, log_context_)
            << task_name_ << " failed with " << ToStatus(result)
            << " for Attempt " << i;
      }
      const absl::Duration backoff = ExponentialBackoffForRetry(i);
      if (!sleep_for_.Duration(backoff)) {
        return absl::CancelledError("SleepFor cancelled for retries.");
      }
    }
    return result;
  }

 private:
  Func func_;
  std::string task_name_;
  int max_attempts_;
  const absl::AnyInvocable<void(const absl::Status&, int) const>&
      metrics_callback_;
  const SleepFor& sleep_for_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

// Retries functors that return an absl::StatusOr<T> until they are `ok`.
// The value of type T is returned by this function.
// `metrics_callback` is optional.
template <typename Func>
typename std::invoke_result_t<RetryableWithMax<Func>>::value_type RetryUntilOk(
    Func&& f, std::string task_name,
    const absl::AnyInvocable<void(const absl::Status&, int) const>&
        metrics_callback,
    privacy_sandbox::server_common::log::PSLogContext& log_context =
        const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
            privacy_sandbox::server_common::log::kNoOpContext),
    const UnstoppableSleepFor& sleep_for = UnstoppableSleepFor()) {
  return RetryableWithMax(std::forward<Func>(f), std::move(task_name),
                          RetryableWithMax<Func>::kUnlimitedRetry,
                          metrics_callback, sleep_for, log_context)()
      .value();
}
// Same as above `RetryUntilOk`, wrapped in an `opentelemetry::trace::Span`.
// Each individual retry of `func` is also traced.
// `metrics_callback` is optional.
template <typename Func>
typename std::invoke_result_t<RetryableWithMax<Func>>::value_type
TraceRetryUntilOk(
    Func&& func, std::string task_name,
    const absl::AnyInvocable<void(const absl::Status&, int) const>&
        metrics_callback,
    privacy_sandbox::server_common::log::PSLogContext& log_context =
        const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
            privacy_sandbox::server_common::log::kNoOpContext),
    std::vector<privacy_sandbox::server_common::TelemetryAttribute> attributes =
        {}) {
  auto span = privacy_sandbox::server_common::GetTracer()->StartSpan(
      "RetryUntilOk - " + task_name);
  auto scope = opentelemetry::trace::Scope(span);
  auto wrapped = [func = std::move(func), attributes = std::move(attributes),
                  task_name]() {
    return TraceWithStatusOr(std::move(func), task_name, std::move(attributes));
  };
  return RetryUntilOk(std::move(wrapped), std::move(task_name),
                      metrics_callback, log_context);
}

// Retries functors that return an absl::Status until they are `ok`.
// `metrics_callback` is optional.
inline void RetryUntilOk(
    std::function<absl::Status()> func, std::string task_name,
    const absl::AnyInvocable<void(const absl::Status&, int) const>&
        metrics_callback,
    privacy_sandbox::server_common::log::PSLogContext& log_context =
        const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
            privacy_sandbox::server_common::log::kNoOpContext),
    const UnstoppableSleepFor& sleep_for = UnstoppableSleepFor()) {
  RetryableWithMax(std::move(func), std::move(task_name),
                   RetryableWithMax<decltype(func)>::kUnlimitedRetry,
                   metrics_callback, sleep_for, log_context)()
      .IgnoreError();
}

// Starts and `opentelemetry::trace::Span` and Calls `RetryUntilOk`.
// Each individual retry of `func` is also traced.
// `metrics_callback` is optional.
void TraceRetryUntilOk(
    std::function<absl::Status()> func, std::string task_name,
    const absl::AnyInvocable<void(const absl::Status&, int) const>&
        metrics_callback,
    privacy_sandbox::server_common::log::PSLogContext& log_context =
        const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
            privacy_sandbox::server_common::log::kNoOpContext));

// Retries functors that return an absl::StatusOr<T> until they are `ok` or
// max_attempts is reached. Retry starts at 1.
// `metrics_callback` is optional.
template <typename Func>
typename std::invoke_result_t<RetryableWithMax<Func>> RetryWithMax(
    Func&& f, std::string task_name, int max_attempts,
    const absl::AnyInvocable<void(const absl::Status&, int) const>&
        metrics_callback,
    const SleepFor& sleep_for,
    privacy_sandbox::server_common::log::PSLogContext& log_context =
        const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
            privacy_sandbox::server_common::log::kNoOpContext)) {
  return RetryableWithMax(std::forward<Func>(f), std::move(task_name),
                          max_attempts, metrics_callback, sleep_for,
                          log_context)();
}
}  // namespace kv_server

#endif  // COMPONENTS_ERRORS_RETRY_H_
