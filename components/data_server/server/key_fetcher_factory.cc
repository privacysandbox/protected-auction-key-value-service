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

#include "components/data_server/server/key_fetcher_factory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "glog/logging.h"
#include "src/core/lib/event_engine/default_event_engine.h"
#include "src/cpp/concurrent/event_engine_executor.h"
#include "src/cpp/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"
#include "src/cpp/encryption/key_fetcher/src/fake_key_fetcher_manager.h"

namespace kv_server {
using ::privacy_sandbox::server_common::EventEngineExecutor;
using ::privacy_sandbox::server_common::KeyFetcherManagerFactory;
using ::privacy_sandbox::server_common::KeyFetcherManagerInterface;
using ::privacy_sandbox::server_common::PrivateKeyFetcherFactory;
using ::privacy_sandbox::server_common::PrivateKeyFetcherInterface;
using ::privacy_sandbox::server_common::PublicKeyFetcherFactory;
using ::privacy_sandbox::server_common::PublicKeyFetcherInterface;

namespace {
constexpr absl::string_view kUseRealCoordinatorsParameterSuffix =
    "use-real-coordinators";
constexpr absl::string_view kPublicKeyEndpointParameterSuffix =
    "public-key-endpoint";
constexpr absl::string_view
    kPrimaryCoordinatorPrivateKeyEndpointParameterSuffix =
        "primary-coordinator-private-key-endpoint";
constexpr absl::string_view
    kSecondaryCoordinatorPrivateKeyEndpointParameterSuffix =
        "secondary-coordinator-private-key-endpoint";
constexpr absl::string_view kPrimaryCoordinatorAccountIdentityParameterSuffix =
    "primary-coordinator-account-identity";
constexpr absl::string_view
    kSecondaryCoordinatorAccountIdentityParameterSuffix =
        "secondary-coordinator-account-identity";
constexpr absl::string_view kPrimaryCoordinatorRegionParameterSuffix =
    "primary-coordinator-region";
constexpr absl::string_view kSecondaryCoordinatorRegionParameterSuffix =
    "secondary-coordinator-region";
// Setting these to match
// ..fledge/servers/bidding-auction-server/+/main:services/common/constants/common_service_flags.cc
constexpr absl::Duration kPrivateKeyCacheTtl = absl::Hours(24 * 45);  // 45 days
constexpr absl::Duration kKeyRefreshFlowRunFrequency = absl::Hours(3);
}  // namespace

std::unique_ptr<privacy_sandbox::server_common::KeyFetcherManagerInterface>
CreateKeyFetcherManager(const ParameterFetcher& parameter_fetcher) {
  // Note that currently `use-real-coordinator` parameter can be changed
  // dynamically. Theoretically, if this change resulted in the immediate
  // reconstruction of the key fetcher manager some potential clever attacks
  // could be done, even though they sound hard
  // to execute. E.g. have a UDF function take a long time, during which an
  // AdTech updates the key fetecher manager interface. In the sharded world
  // the downstream requests would then be encrypted with known test keys.
  // If an ability to reconstrcut the manager on the fly will be implemented in
  // the future, one way to mitigate this attack would be to change the
  // parameter to a flag. And then explicitly pass that flag in a script so that
  // it affects the PCR0.
  if (!parameter_fetcher.GetBoolParameter(
          kUseRealCoordinatorsParameterSuffix)) {
    LOG(INFO) << "Not using real coordinators. Using hardcoded unsafe public "
                 "and private keys";
    return std::make_unique<
        privacy_sandbox::server_common::FakeKeyFetcherManager>();
  }

  auto publicKeyEndpointParameter =
      parameter_fetcher.GetParameter(kPublicKeyEndpointParameterSuffix);
  LOG(INFO) << "Retrieved " << kPublicKeyEndpointParameterSuffix
            << " parameter: " << publicKeyEndpointParameter;
  std::vector<std::string> endpoints = {publicKeyEndpointParameter};
  std::unique_ptr<PublicKeyFetcherInterface> public_key_fetcher =
      PublicKeyFetcherFactory::Create(endpoints);
  google::scp::cpio::PrivateKeyVendingEndpoint primary, secondary;
  primary.account_identity = parameter_fetcher.GetParameter(
      kPrimaryCoordinatorAccountIdentityParameterSuffix);
  LOG(INFO) << "Retrieved " << kPrimaryCoordinatorAccountIdentityParameterSuffix
            << " parameter: " << primary.account_identity;
  primary.private_key_vending_service_endpoint = parameter_fetcher.GetParameter(
      kPrimaryCoordinatorPrivateKeyEndpointParameterSuffix);
  LOG(INFO) << "Retrieved "
            << kPrimaryCoordinatorPrivateKeyEndpointParameterSuffix
            << " parameter: " << primary.private_key_vending_service_endpoint;

  primary.service_region =
      parameter_fetcher.GetParameter(kPrimaryCoordinatorRegionParameterSuffix);
  LOG(INFO) << "Retrieved " << kPrimaryCoordinatorRegionParameterSuffix
            << " parameter: " << primary.service_region;

  secondary.account_identity = parameter_fetcher.GetParameter(
      kSecondaryCoordinatorAccountIdentityParameterSuffix);
  LOG(INFO) << "Retrieved "
            << kSecondaryCoordinatorAccountIdentityParameterSuffix
            << " parameter: " << secondary.account_identity;
  secondary.private_key_vending_service_endpoint =
      parameter_fetcher.GetParameter(
          kSecondaryCoordinatorPrivateKeyEndpointParameterSuffix);
  LOG(INFO) << "Retrieved "
            << kSecondaryCoordinatorPrivateKeyEndpointParameterSuffix
            << " parameter: " << secondary.private_key_vending_service_endpoint;
  secondary.service_region = parameter_fetcher.GetParameter(
      kSecondaryCoordinatorRegionParameterSuffix);
  LOG(INFO) << "Retrieved " << kSecondaryCoordinatorRegionParameterSuffix
            << " parameter: " << secondary.service_region;

  std::unique_ptr<PrivateKeyFetcherInterface> private_key_fetcher =
      PrivateKeyFetcherFactory::Create(primary, {secondary},
                                       kPrivateKeyCacheTtl);
  auto event_engine = std::make_unique<EventEngineExecutor>(
      grpc_event_engine::experimental::GetDefaultEventEngine());
  std::unique_ptr<KeyFetcherManagerInterface> manager =
      KeyFetcherManagerFactory::Create(
          kKeyRefreshFlowRunFrequency, std::move(public_key_fetcher),
          std::move(private_key_fetcher), std::move(event_engine));
  manager->Start();

  return manager;
}
}  // namespace kv_server
