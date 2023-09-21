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

#include <memory>

#ifndef COMPONENTS_DATA_SERVER_SERVER_KEY_FETCHER_FACTORY_H_
#define COMPONENTS_DATA_SERVER_SERVER_KEY_FETCHER_FACTORY_H_

#include "components/data_server/server/parameter_fetcher.h"
#include "src/cpp/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"

namespace kv_server {
// Constructs KeyFetcherManager.
// Example:
// auto factory = KeyFetcherFactory::Create();
// auto key_fetcher_manager =
//         factory->CreateKeyFetcherManager(parameter_fetcher);
class KeyFetcherFactory {
 public:
  virtual ~KeyFetcherFactory() = default;
  // Creates KeyFetcherManager.
  virtual std::unique_ptr<
      privacy_sandbox::server_common::KeyFetcherManagerInterface>
  CreateKeyFetcherManager(const ParameterFetcher& parameter_fetcher) const = 0;
  // Constructs a KeyFetcherFactory.
  static std::unique_ptr<KeyFetcherFactory> Create();
};

// Constructs CloudKeyFetcherManager. CloudKeyFetcherManager has common logic
// for cloud implementations: GCP and AWS. For all usecases KeyFetcherFactory
// should be used.
class CloudKeyFetcherFactory : public KeyFetcherFactory {
 public:
  // Creates KeyFetcherManager.
  std::unique_ptr<privacy_sandbox::server_common::KeyFetcherManagerInterface>
  CreateKeyFetcherManager(
      const ParameterFetcher& parameter_fetcher) const override;

 protected:
  virtual google::scp::cpio::PrivateKeyVendingEndpoint
  GetPrimaryKeyFetchingEndpoint(
      const ParameterFetcher& parameter_fetcher) const;
  virtual google::scp::cpio::PrivateKeyVendingEndpoint
  GetSecondaryKeyFetchingEndpoint(
      const ParameterFetcher& parameter_fetcher) const;
  virtual ::privacy_sandbox::server_common::CloudPlatform GetCloudPlatform()
      const;
};

}  // namespace kv_server
#endif  // COMPONENTS_DATA_SERVER_SERVER_KEY_FETCHER_FACTORY_H_
