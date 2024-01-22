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

RequestContext::RequestContext(std::string request_id)
    : request_id_(std::move(request_id)) {
  // Create a metrics context in the context map and
  // associated it with request id
  KVServerContextMap()->Get(&request_id_);
  CHECK_OK([this]() {
    // Remove the the metrics context for request_id to transfer the ownership
    // of metrics context to the RequestContext. This is to ensure that metrics
    // context has the same lifetime with RequestContext and be destroyed when
    // RequestContext go out of scope.
    PS_ASSIGN_OR_RETURN(metrics_context_,
                        KVServerContextMap()->Remove(&request_id_));
    return absl::OkStatus();
  }()) << "Metrics context is not initialized";
}

KVMetricsContext* RequestContext::GetMetricsContext() const {
  return metrics_context_.get();
}

}  // namespace kv_server
