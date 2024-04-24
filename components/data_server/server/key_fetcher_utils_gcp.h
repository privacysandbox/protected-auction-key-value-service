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

#ifndef COMPONENTS_DATA_SERVER_KEY_FETCHER_UTILS_GCP_H_
#define COMPONENTS_DATA_SERVER_KEY_FETCHER_UTILS_GCP_H_

#include "components/data_server/server/parameter_fetcher.h"
#include "src/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"

namespace kv_server {

using ::google::scp::cpio::PrivateKeyVendingEndpoint;

constexpr std::string_view kPrimaryKeyServiceCloudFunctionUrlSuffix =
    "primary-key-service-cloud-function-url";
constexpr std::string_view kPrimaryWorkloadIdentityPoolProviderSuffix =
    "primary-workload-identity-pool-provider";
constexpr std::string_view kSecondaryKeyServiceCloudFunctionUrlSuffix =
    "secondary-key-service-cloud-function-url";
constexpr std::string_view kSecondaryWorkloadIdentityPoolProviderSuffix =
    "secondary-workload-identity-pool-provider";

void UpdatePrimaryGcpEndpoint(
    PrivateKeyVendingEndpoint& endpoint,
    const ParameterFetcher& parameter_fetcher,
    privacy_sandbox::server_common::log::PSLogContext& log_context);

void UpdateSecondaryGcpEndpoint(
    PrivateKeyVendingEndpoint& endpoint,
    const ParameterFetcher& parameter_fetcher,
    privacy_sandbox::server_common::log::PSLogContext& log_context);

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_KEY_FETCHER_UTILS_GCP_H_
