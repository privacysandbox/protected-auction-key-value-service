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

#ifndef PUBLIC_APPLICATIONS_PAS_RETRIEVAL_REQUEST_BUILDER_H_
#define PUBLIC_APPLICATIONS_PAS_RETRIEVAL_REQUEST_BUILDER_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "public/query/v2/get_values_v2.pb.h"

namespace kv_server::application_pas {

// Builds a GetValuesRequest. Stores the input arguments into the request.
//
// Input strings must be JSON-compliant, i.e., for binary strings, they must be
// base64 encoded.
v2::GetValuesRequest BuildRetrievalRequest(
    const privacy_sandbox::server_common::LogContext& log_context,
    const privacy_sandbox::server_common::ConsentedDebugConfiguration&
        consented_debug_config,
    std::string protected_signals,
    absl::flat_hash_map<std::string, std::string> device_metadata,
    std::string contextual_signals,
    std::vector<std::string> optional_ad_ids = {});

// Builds a GetValuesRequest. Stores the input arguments into the request.
v2::GetValuesRequest BuildLookupRequest(
    const privacy_sandbox::server_common::LogContext& log_context,
    const privacy_sandbox::server_common::ConsentedDebugConfiguration&
        consented_debug_config,
    std::vector<std::string> ad_ids);

}  // namespace kv_server::application_pas

#endif  // PUBLIC_APPLICATIONS_PAS_RETRIEVAL_REQUEST_BUILDER_H_
