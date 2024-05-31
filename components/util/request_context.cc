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

#include "components/util/request_context.h"

#include <utility>

#include "components/telemetry/server_definition.h"

namespace kv_server {
namespace {
constexpr char kGenerationId[] = "generationId";
constexpr char kAdtechDebugId[] = "adtechDebugId";
constexpr char kDefaultConsentedGenerationId[] = "consented";
}  // namespace

UdfRequestMetricsContext& RequestContext::GetUdfRequestMetricsContext() const {
  return udf_request_metrics_context_;
}
InternalLookupMetricsContext& RequestContext::GetInternalLookupMetricsContext()
    const {
  return internal_lookup_metrics_context_;
}
RequestLogContext& RequestContext::GetRequestLogContext() const {
  return *request_log_context_;
}
void RequestContext::UpdateLogContext(
    const privacy_sandbox::server_common::LogContext& log_context,
    const privacy_sandbox::server_common::ConsentedDebugConfiguration&
        consented_debug_config) {
  request_log_context_ =
      std::make_unique<RequestLogContext>(log_context, consented_debug_config);
  if (request_log_context_->GetRequestLoggingContext().is_consented()) {
    const std::string generation_id =
        request_log_context_->GetLogContext().generation_id().empty()
            ? kDefaultConsentedGenerationId
            : request_log_context_->GetLogContext().generation_id();
    udf_request_metrics_context_.SetConsented(generation_id);
    internal_lookup_metrics_context_.SetConsented(generation_id);
  }
}
RequestContext::RequestContext(
    const privacy_sandbox::server_common::LogContext& log_context,
    const privacy_sandbox::server_common::ConsentedDebugConfiguration&
        consented_debug_config,
    std::string request_id)
    : request_id_(request_id),
      udf_request_metrics_context_(KVServerContextMap()->Get(&request_id_)),
      internal_lookup_metrics_context_(
          InternalLookupServerContextMap()->Get(&request_id_)) {
  request_log_context_ =
      std::make_unique<RequestLogContext>(log_context, consented_debug_config);
}

privacy_sandbox::server_common::log::ContextImpl<>&
RequestContext::GetPSLogContext() const {
  return request_log_context_->GetRequestLoggingContext();
}

RequestLogContext::RequestLogContext(
    const privacy_sandbox::server_common::LogContext& log_context,
    const privacy_sandbox::server_common::ConsentedDebugConfiguration&
        consented_debug_config)
    : log_context_(log_context),
      consented_debug_config_(consented_debug_config),
      request_logging_context_(GetContextMap(log_context),
                               consented_debug_config) {}

privacy_sandbox::server_common::log::ContextImpl<>&
RequestLogContext::GetRequestLoggingContext() {
  return request_logging_context_;
}
const privacy_sandbox::server_common::LogContext&
RequestLogContext::GetLogContext() const {
  return log_context_;
}
const privacy_sandbox::server_common::ConsentedDebugConfiguration&
RequestLogContext::GetConsentedDebugConfiguration() const {
  return consented_debug_config_;
}
absl::btree_map<std::string, std::string> RequestLogContext::GetContextMap(
    const privacy_sandbox::server_common::LogContext& log_context) {
  return {{kGenerationId, log_context.generation_id()},
          {kAdtechDebugId, log_context.adtech_debug_id()}};
}
RequestContextFactory::RequestContextFactory(
    const privacy_sandbox::server_common::LogContext& log_context,
    const privacy_sandbox::server_common::ConsentedDebugConfiguration&
        consented_debug_config) {
  request_context_ =
      std::make_shared<RequestContext>(log_context, consented_debug_config);
}
std::weak_ptr<RequestContext> RequestContextFactory::GetWeakCopy() const {
  return request_context_;
}
const RequestContext& RequestContextFactory::Get() const {
  return *request_context_;
}

void RequestContextFactory::UpdateLogContext(
    const privacy_sandbox::server_common::LogContext& log_context,
    const privacy_sandbox::server_common::ConsentedDebugConfiguration&
        consented_debug_config) {
  request_context_->UpdateLogContext(log_context, consented_debug_config);
}

}  // namespace kv_server
