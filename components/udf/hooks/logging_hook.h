/*
 * Copyright 2023 Google LLC
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

#ifndef COMPONENTS_UDF_LOGGING_HOOK_H_
#define COMPONENTS_UDF_LOGGING_HOOK_H_

#include <memory>
#include <string>
#include <tuple>

#include "absl/log/log.h"
#include "components/util/request_context.h"

namespace kv_server {

// UDF hook for logging a string.
// TODO(b/285331079): Disable for production builds.
inline void LogMessage(
    google::scp::roma::FunctionBindingPayload<std::weak_ptr<RequestContext>>&
        payload) {
  std::shared_ptr<RequestContext> request_context = payload.metadata.lock();
  if (request_context == nullptr) {
    PS_VLOG(1) << "Request context is not available, the request might "
                  "have been marked as complete";
    return;
  }
  PS_VLOG(10, request_context->GetPSLogContext()) << "Called logging hook";
  PS_LOG(INFO, request_context->GetPSLogContext())
      << payload.io_proto.input_string();
}

}  // namespace kv_server

#endif  // COMPONENTS_UDF_LOGGING_HOOK_H_
