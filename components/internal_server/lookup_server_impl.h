/*
 * Copyright 2022 Google LLC
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

#ifndef COMPONENTS_INTERNAL_SERVER_LOOKUP_SERVER_IMPL_H_
#define COMPONENTS_INTERNAL_SERVER_LOOKUP_SERVER_IMPL_H_

#include "components/data_server/cache/cache.h"
#include "components/internal_server/lookup.grpc.pb.h"
#include "grpcpp/grpcpp.h"
#include "src/cpp/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"

namespace kv_server {
// Implements the internal lookup service for the data store.
class LookupServiceImpl final
    : public kv_server::InternalLookupService::Service {
 public:
  LookupServiceImpl(const Cache& cache,
                    privacy_sandbox::server_common::KeyFetcherManagerInterface&
                        key_fetcher_manager)
      : cache_(cache), key_fetcher_manager_(key_fetcher_manager) {}

  ~LookupServiceImpl() override = default;

  grpc::Status InternalLookup(
      grpc::ServerContext* context,
      const kv_server::InternalLookupRequest* request,
      kv_server::InternalLookupResponse* response) override;

  grpc::Status SecureLookup(grpc::ServerContext* context,
                            const kv_server::SecureLookupRequest* request,
                            kv_server::SecureLookupResponse* response) override;

  grpc::Status InternalRunQuery(
      grpc::ServerContext* context,
      const kv_server::InternalRunQueryRequest* request,
      kv_server::InternalRunQueryResponse* response) override;

 private:
  const Cache& cache_;
  privacy_sandbox::server_common::KeyFetcherManagerInterface&
      key_fetcher_manager_;
};

}  // namespace kv_server

#endif  // COMPONENTS_INTERNAL_SERVER_LOOKUP_SERVER_IMPL_H_
