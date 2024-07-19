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

#ifndef COMPONENTS_DATA_SERVER_SERVER_INITIALIZER_H_
#define COMPONENTS_DATA_SERVER_SERVER_INITIALIZER_H_

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/data_server/server/parameter_fetcher.h"
#include "components/internal_server/lookup.h"
#include "components/sharding/cluster_mappings_manager.h"
#include "components/udf/hooks/get_values_hook.h"
#include "components/udf/hooks/run_query_hook.h"
#include "grpcpp/grpcpp.h"
#include "public/sharding/key_sharder.h"
#include "src/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"

namespace kv_server {

// Internal Sharded Lookup Server --
// if `num_shards` > 1, then serves requests originating from servers with
// a different `shard_num`. Only has data for `shard_num` assigned to the
// server at the start up. if `num_shards` == 1, then null, since no remote
// lookups are necessray
struct RemoteLookup {
  std::unique_ptr<grpc::Service> remote_lookup_service;
  std::unique_ptr<grpc::Server> remote_lookup_server;
};

struct ShardManagerState {
  std::unique_ptr<ClusterMappingsManager> cluster_mappings_manager;
  std::unique_ptr<ShardManager> shard_manager;
};

// Encapsulates logic that differs for sharded and non-sharded implementations.
class ServerInitializer {
 public:
  virtual RemoteLookup CreateAndStartRemoteLookupServer() = 0;
  virtual absl::StatusOr<ShardManagerState> InitializeUdfHooks(
      GetValuesHook& string_get_values_hook,
      GetValuesHook& binary_get_values_hook,
      RunSetQueryStringHook& run_set_query_string_hook,
      RunSetQueryUInt32Hook& run_set_query_uint32_hook,
      RunSetQueryUInt64Hook& run_set_query_uint64_hook) = 0;
};

std::unique_ptr<ServerInitializer> GetServerInitializer(
    int64_t num_shards,
    privacy_sandbox::server_common::KeyFetcherManagerInterface&
        key_fetcher_manager,
    Lookup& local_lookup, std::string environment, int32_t current_shard_num,
    InstanceClient& instance_client, Cache& cache,
    ParameterFetcher& parameter_fetcher, KeySharder key_sharder,
    privacy_sandbox::server_common::log::PSLogContext& log_context =
        const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
            privacy_sandbox::server_common::log::kNoOpContext));

}  // namespace kv_server
#endif  // COMPONENTS_DATA_SERVER_SERVER_INITIALIZER_H_
