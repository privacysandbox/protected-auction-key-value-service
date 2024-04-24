/*
 * Copyright 2024 Google LLC
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

#include "components/data_server/server/key_fetcher_utils_gcp.h"

#include "absl/log/log.h"

namespace kv_server {

void SetGcpSpecificParameters(
    PrivateKeyVendingEndpoint& endpoint,
    const ParameterFetcher& parameter_fetcher,
    const std::string_view cloudfunction_prefix,
    const std::string_view wip_provider,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  endpoint.gcp_private_key_vending_service_cloudfunction_url =
      parameter_fetcher.GetParameter(cloudfunction_prefix);
  PS_LOG(INFO, log_context)
      << "Retrieved " << cloudfunction_prefix << " parameter: "
      << endpoint.gcp_private_key_vending_service_cloudfunction_url;
  endpoint.gcp_wip_provider = parameter_fetcher.GetParameter(wip_provider);
  PS_LOG(INFO, log_context) << "Retrieved " << wip_provider
                            << " parameter: " << endpoint.gcp_wip_provider;
}

void UpdatePrimaryGcpEndpoint(
    PrivateKeyVendingEndpoint& endpoint,
    const ParameterFetcher& parameter_fetcher,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  SetGcpSpecificParameters(
      endpoint, parameter_fetcher, kPrimaryKeyServiceCloudFunctionUrlSuffix,
      kPrimaryWorkloadIdentityPoolProviderSuffix, log_context);
}

void UpdateSecondaryGcpEndpoint(
    PrivateKeyVendingEndpoint& endpoint,
    const ParameterFetcher& parameter_fetcher,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  SetGcpSpecificParameters(
      endpoint, parameter_fetcher, kSecondaryKeyServiceCloudFunctionUrlSuffix,
      kSecondaryWorkloadIdentityPoolProviderSuffix, log_context);
}

}  // namespace kv_server
