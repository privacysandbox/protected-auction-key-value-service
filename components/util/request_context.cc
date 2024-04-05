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

UdfRequestMetricsContext& RequestContext::GetUdfRequestMetricsContext() const {
  return udf_request_metrics_context_;
}
InternalLookupMetricsContext& RequestContext::GetInternalLookupMetricsContext()
    const {
  return internal_lookup_metrics_context_;
}

}  // namespace kv_server
