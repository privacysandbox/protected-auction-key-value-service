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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "glog/logging.h"
#include "src/core/lib/event_engine/default_event_engine.h"
#include "src/cpp/concurrent/event_engine_executor.h"
#include "src/cpp/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"
#include "src/cpp/encryption/key_fetcher/src/fake_key_fetcher_manager.h"

ABSL_FLAG(std::string, public_key_endpoint, "", "Public key endpoint.");
ABSL_FLAG(std::string, primary_coordinator_private_key_endpoint, "",
          "Primary coordinator private key endpoint.");
ABSL_FLAG(std::string, secondary_coordinator_private_key_endpoint, "",
          "Secondary coordinator private key endpoint.");
ABSL_FLAG(std::string, primary_coordinator_region, "",
          "Primary coordinator region.");
ABSL_FLAG(std::string, secondary_coordinator_region, "",
          "Secondary coordinator region.");

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
constexpr absl::string_view kPrimaryCoordinatorAccountIdentityParameterSuffix =
    "primary-coordinator-account-identity";
constexpr absl::string_view
    kSecondaryCoordinatorAccountIdentityParameterSuffix =
        "secondary-coordinator-account-identity";
// Setting these to match
// ..fledge/servers/bidding-auction-server/+/main:services/common/constants/common_service_flags.cc
constexpr absl::Duration kPrivateKeyCacheTtl = absl::Hours(24 * 45);  // 45 days
constexpr absl::Duration kKeyRefreshFlowRunFrequency = absl::Hours(3);
}  // namespace

std::unique_ptr<privacy_sandbox::server_common::KeyFetcherManagerInterface>
CreateKeyFetcherManager(const ParameterFetcher& parameter_fetcher) {
  if (!parameter_fetcher.GetBoolParameter(
          kUseRealCoordinatorsParameterSuffix)) {
    LOG(INFO) << "Not using real coordinators. Using hardcoded unsafe public "
                 "and private keys";
    return std::make_unique<
        privacy_sandbox::server_common::FakeKeyFetcherManager>();
  }
  auto publicKeyEndpointParameter = absl::GetFlag(FLAGS_public_key_endpoint);
  LOG(INFO) << "Retrieved public_key_endpoint parameter: "
            << publicKeyEndpointParameter;
  std::vector<std::string> endpoints = {publicKeyEndpointParameter};
  std::unique_ptr<PublicKeyFetcherInterface> public_key_fetcher =
      PublicKeyFetcherFactory::Create(endpoints);
  google::scp::cpio::PrivateKeyVendingEndpoint primary, secondary;
  primary.account_identity = parameter_fetcher.GetParameter(
      kPrimaryCoordinatorAccountIdentityParameterSuffix);
  LOG(INFO) << "Retrieved " << kPrimaryCoordinatorAccountIdentityParameterSuffix
            << " parameter: " << primary.account_identity;
  primary.private_key_vending_service_endpoint =
      absl::GetFlag(FLAGS_primary_coordinator_private_key_endpoint);
  LOG(INFO) << "Retrieved primary_coordinator_private_key_endpoint parameter: "
            << primary.private_key_vending_service_endpoint;
  primary.service_region = absl::GetFlag(FLAGS_primary_coordinator_region);
  LOG(INFO) << "Retrieved primary_coordinator_region parameter: "
            << primary.service_region;
  secondary.account_identity = parameter_fetcher.GetParameter(
      kSecondaryCoordinatorAccountIdentityParameterSuffix);
  LOG(INFO) << "Retrieved "
            << kSecondaryCoordinatorAccountIdentityParameterSuffix
            << " parameter: " << secondary.account_identity;
  secondary.private_key_vending_service_endpoint =
      absl::GetFlag(FLAGS_secondary_coordinator_private_key_endpoint);
  LOG(INFO)
      << "Retrieved secondary_coordinator_private_key_endpoint parameter: "
      << secondary.private_key_vending_service_endpoint;
  secondary.service_region = absl::GetFlag(FLAGS_secondary_coordinator_region);
  LOG(INFO) << "Retrieved secondary_coordinator_region parameter: "
            << secondary.service_region;
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
