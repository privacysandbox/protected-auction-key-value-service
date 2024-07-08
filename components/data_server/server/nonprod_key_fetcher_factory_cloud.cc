// Copyright 2024 Google LLC
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

#include "components/data_server/server/nonprod_key_fetcher_factory_cloud.h"

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

namespace kv_server {
using ::google::scp::cpio::PrivateKeyVendingEndpoint;

constexpr std::string_view kPublicKeyEndpointParameterSuffix =
    "public-key-endpoint";

std::vector<std::string>
NonprodCloudKeyFetcherFactory::GetPublicKeyFetchingEndpoint(
    const ParameterFetcher& parameter_fetcher) const {
  auto publicKeyEndpointParameter =
      parameter_fetcher.GetParameter(kPublicKeyEndpointParameterSuffix);
  PS_LOG(INFO, log_context_) << "Retrieved public_key_endpoint parameter: "
                             << publicKeyEndpointParameter;
  std::vector<std::string> endpoints = {publicKeyEndpointParameter};
  return endpoints;
}
}  // namespace kv_server
