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
#include <utility>

#include "absl/flags/flag.h"
#include "components/data_server/server/key_fetcher_factory.h"
#include "src/encryption/key_fetcher/fake_key_fetcher_manager.h"

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
using ::privacy_sandbox::server_common::FakeKeyFetcherManager;
using ::privacy_sandbox::server_common::KeyFetcherManagerInterface;

class LocalKeyFetcherFactory : public KeyFetcherFactory {
  std::unique_ptr<KeyFetcherManagerInterface> CreateKeyFetcherManager(
      const ParameterFetcher& parameter_fetcher) const override {
    return std::make_unique<FakeKeyFetcherManager>();
  }
};

std::unique_ptr<KeyFetcherFactory> KeyFetcherFactory::Create(
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<LocalKeyFetcherFactory>();
}

}  // namespace kv_server
