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

#ifndef COMPONENTS_UTIL_REQUEST_CONTEXT_H_
#define COMPONENTS_UTIL_REQUEST_CONTEXT_H_

#include <memory>
#include <string>
#include <utility>

#include "components/telemetry/server_definition.h"
#include "src/logger/request_context_impl.h"

namespace kv_server {

// RequestLogContext holds value of LogContext and ConsentedDebugConfiguration
// passed from the upstream application.
class RequestLogContext {
 public:
  explicit RequestLogContext(
      const privacy_sandbox::server_common::LogContext& log_context,
      const privacy_sandbox::server_common::ConsentedDebugConfiguration&
          consented_debug_config);

  privacy_sandbox::server_common::log::ContextImpl& GetRequestLoggingContext();

  const privacy_sandbox::server_common::LogContext& GetLogContext() const;

  const privacy_sandbox::server_common::ConsentedDebugConfiguration&
  GetConsentedDebugConfiguration() const;

 private:
  // Parses the LogContext to btree map which is used to construct request
  // logging context and used as labels for logging messages
  absl::btree_map<std::string, std::string> GetContextMap(
      const privacy_sandbox::server_common::LogContext& log_context);
  const privacy_sandbox::server_common::LogContext log_context_;
  const privacy_sandbox::server_common::ConsentedDebugConfiguration
      consented_debug_config_;
  privacy_sandbox::server_common::log::ContextImpl request_logging_context_;
};

// RequestContext holds the reference of udf request metrics context,
// internal lookup request context, and request log context
// that ties to a single request. The request_id can be either passed
// from upper stream or assigned from uuid generated when
// RequestContext is constructed.

class RequestContext {
 public:
  explicit RequestContext(const ScopeMetricsContext& metrics_context,
                          RequestLogContext& request_log_context)
      : udf_request_metrics_context_(
            metrics_context.GetUdfRequestMetricsContext()),
        internal_lookup_metrics_context_(
            metrics_context.GetInternalLookupMetricsContext()),
        request_log_context_(request_log_context) {}
  UdfRequestMetricsContext& GetUdfRequestMetricsContext() const;
  InternalLookupMetricsContext& GetInternalLookupMetricsContext() const;
  RequestLogContext& GetRequestLogContext() const;

  ~RequestContext() = default;

 private:
  UdfRequestMetricsContext& udf_request_metrics_context_;
  InternalLookupMetricsContext& internal_lookup_metrics_context_;
  RequestLogContext& request_log_context_;
};

}  // namespace kv_server

#endif  // COMPONENTS_UTIL_REQUEST_CONTEXT_H_
