// Copyright 2023 Google LLC
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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/log.h"
#include "components/data_server/server/key_fetcher_factory.h"
#include "src/concurrent/event_engine_executor.h"
#include "src/core/lib/event_engine/default_event_engine.h"
#include "src/encryption/key_fetcher/fake_key_fetcher_manager.h"
#include "src/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"

ABSL_FLAG(std::string, public_key_endpoint, "", "Public key endpoint.");

namespace kv_server {
using ::google::scp::cpio::PrivateKeyVendingEndpoint;
using ::privacy_sandbox::server_common::CloudPlatform;
using ::privacy_sandbox::server_common::EventEngineExecutor;
using ::privacy_sandbox::server_common::FakeKeyFetcherManager;
using ::privacy_sandbox::server_common::KeyFetcherManagerFactory;
using ::privacy_sandbox::server_common::KeyFetcherManagerInterface;
using ::privacy_sandbox::server_common::PrivateKeyFetcherFactory;
using ::privacy_sandbox::server_common::PrivateKeyFetcherInterface;
using ::privacy_sandbox::server_common::PublicKeyFetcherFactory;
using ::privacy_sandbox::server_common::PublicKeyFetcherInterface;

namespace {
constexpr std::string_view kUseRealCoordinatorsParameterSuffix =
    "use-real-coordinators";
constexpr std::string_view kPrimaryCoordinatorAccountIdentityParameterSuffix =
    "primary-coordinator-account-identity";
constexpr std::string_view kSecondaryCoordinatorAccountIdentityParameterSuffix =
    "secondary-coordinator-account-identity";
constexpr std::string_view
    kPrimaryCoordinatorPrivateKeyEndpointParameterSuffix =
        "primary-coordinator-private-key-endpoint";
constexpr std::string_view kPrimaryCoordinatorRegionParameterSuffix =
    "primary-coordinator-region";
constexpr std::string_view
    kSecondaryCoordinatorPrivateKeyEndpointParameterSuffix =
        "secondary-coordinator-private-key-endpoint";
constexpr std::string_view kSecondaryCoordinatorRegionParameterSuffix =
    "secondary-coordinator-region";

// Setting these to match
// ..fledge/servers/bidding-auction-server/+/main:services/common/constants/common_service_flags.cc
constexpr absl::Duration kPrivateKeyCacheTtl = absl::Hours(24 * 45);  // 45 days
constexpr absl::Duration kKeyRefreshFlowRunFrequency = absl::Hours(3);

PrivateKeyVendingEndpoint GetKeyFetchingEndpoint(
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    const ParameterFetcher& parameter_fetcher,
    std::string_view account_identity_prefix,
    std::string_view private_key_endpoint_prefix,
    absl::string_view region_prefix) {
  PrivateKeyVendingEndpoint endpoint;
  endpoint.account_identity =
      parameter_fetcher.GetParameter(account_identity_prefix);
  PS_LOG(INFO, log_context) << "Retrieved " << account_identity_prefix
                            << " parameter: " << endpoint.account_identity;
  endpoint.private_key_vending_service_endpoint =
      parameter_fetcher.GetParameter(private_key_endpoint_prefix);
  PS_LOG(INFO, log_context)
      << "Service endpoint: " << endpoint.private_key_vending_service_endpoint;
  endpoint.service_region = parameter_fetcher.GetParameter(region_prefix);
  PS_LOG(INFO, log_context) << "Region: " << endpoint.service_region;
  return endpoint;
}
}  // namespace

std::unique_ptr<KeyFetcherManagerInterface>
CloudKeyFetcherFactory::CreateKeyFetcherManager(
    const ParameterFetcher& parameter_fetcher) const {
  if (!parameter_fetcher.GetBoolParameter(
          kUseRealCoordinatorsParameterSuffix)) {
    PS_LOG(INFO, log_context_)
        << "Not using real coordinators. Using hardcoded unsafe public "
           "and private keys";
    return std::make_unique<FakeKeyFetcherManager>();
  }
  std::vector<std::string> endpoints =
      GetPublicKeyFetchingEndpoint(parameter_fetcher);
  std::unique_ptr<PublicKeyFetcherInterface> public_key_fetcher =
      PublicKeyFetcherFactory::Create({{GetCloudPlatform(), endpoints}},
                                      log_context_);
  auto primary = GetPrimaryKeyFetchingEndpoint(parameter_fetcher);
  auto secondary = GetSecondaryKeyFetchingEndpoint(parameter_fetcher);
  std::unique_ptr<PrivateKeyFetcherInterface> private_key_fetcher =
      PrivateKeyFetcherFactory::Create(primary, {secondary},
                                       kPrivateKeyCacheTtl, log_context_);
  auto event_engine = std::make_unique<EventEngineExecutor>(
      grpc_event_engine::experimental::GetDefaultEventEngine());
  std::unique_ptr<KeyFetcherManagerInterface> manager =
      KeyFetcherManagerFactory::Create(
          kKeyRefreshFlowRunFrequency, std::move(public_key_fetcher),
          std::move(private_key_fetcher), std::move(event_engine));
  manager->Start();

  return manager;
}

std::vector<std::string> CloudKeyFetcherFactory::GetPublicKeyFetchingEndpoint(
    const ParameterFetcher& parameter_fetcher) const {
  auto publicKeyEndpointParameter = absl::GetFlag(FLAGS_public_key_endpoint);
  PS_LOG(INFO, log_context_) << "Retrieved public_key_endpoint parameter: "
                             << publicKeyEndpointParameter;
  std::vector<std::string> endpoints = {publicKeyEndpointParameter};
  return endpoints;
}

PrivateKeyVendingEndpoint CloudKeyFetcherFactory::GetPrimaryKeyFetchingEndpoint(
    const ParameterFetcher& parameter_fetcher) const {
  return GetKeyFetchingEndpoint(
      log_context_, parameter_fetcher,
      kPrimaryCoordinatorAccountIdentityParameterSuffix,
      kPrimaryCoordinatorPrivateKeyEndpointParameterSuffix,
      kPrimaryCoordinatorRegionParameterSuffix);
}

PrivateKeyVendingEndpoint
CloudKeyFetcherFactory::GetSecondaryKeyFetchingEndpoint(
    const ParameterFetcher& parameter_fetcher) const {
  return GetKeyFetchingEndpoint(
      log_context_, parameter_fetcher,
      kSecondaryCoordinatorAccountIdentityParameterSuffix,
      kSecondaryCoordinatorPrivateKeyEndpointParameterSuffix,
      kSecondaryCoordinatorRegionParameterSuffix);
}

CloudPlatform CloudKeyFetcherFactory::GetCloudPlatform() const {
  return CloudPlatform::kAws;
}
}  // namespace kv_server
