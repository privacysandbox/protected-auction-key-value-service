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

#ifndef COMPONENTS_UDF_CUSTOM_METRIC_HOOK_H_
#define COMPONENTS_UDF_CUSTOM_METRIC_HOOK_H_

#include <memory>
#include <string>
#include <tuple>

#include "absl/log/log.h"
#include "components/util/request_context.h"

namespace kv_server {
inline constexpr std::string_view kLogMessageSuccessMsg = "Log metrics success";
// UDF hook for logging BatchUDFMetric. Like other non-custom unsafe metrics,
// custom metrics will be exported noised for non-consented requests, and raw
// for consented requests.
inline void LogCustomMetric(
    google::scp::roma::FunctionBindingPayload<std::weak_ptr<RequestContext>>&
        payload) {
  std::shared_ptr<RequestContext> request_context = payload.metadata.lock();
  if (request_context == nullptr) {
    PS_VLOG(1) << "Request context is not available, the request might "
                  "have been marked as complete";
    return;
  }
  PS_VLOG(1, request_context->GetPSLogContext())
      << "Called log custom metric hook to log metrics";
  privacy_sandbox::server_common::metrics::BatchUDFMetric metrics;
  absl::Status result = google::protobuf::util::JsonStringToMessage(
      payload.io_proto.input_string(), &metrics);
  if (!result.ok()) {
    auto error_msg = absl::StrCat("Failed to parse metrics in Json string:",
                                  payload.io_proto.input_string(),
                                  "\n error: ", result.message());
    payload.io_proto.set_output_string(error_msg);
    PS_VLOG(2, request_context->GetPSLogContext()) << error_msg;
    return;
  }
  result =
      request_context->GetUdfRequestMetricsContext().LogUDFMetrics(metrics);
  if (!result.ok()) {
    auto error_msg =
        absl::StrCat("Failed to log metrics: ", metrics.DebugString(),
                     "\n error: ", result.message());
    payload.io_proto.set_output_string(error_msg);
    PS_VLOG(2, request_context->GetPSLogContext()) << error_msg;
    return;
  }
  payload.io_proto.set_output_string(kLogMessageSuccessMsg);
  PS_VLOG(1, request_context->GetPSLogContext())
      << kLogMessageSuccessMsg << "\n"
      << metrics.DebugString();
}

}  // namespace kv_server

#endif  // COMPONENTS_UDF_CUSTOM_METRIC_HOOK_H_
