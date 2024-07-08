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

#include "components/data_server/server/server_initializer.h"

#include <utility>

#include "absl/log/log.h"
#include "components/internal_server/constants.h"
#include "components/internal_server/local_lookup.h"
#include "components/internal_server/lookup_server_impl.h"
#include "components/internal_server/sharded_lookup.h"
#include "src/encryption/key_fetcher/key_fetcher_manager.h"

namespace kv_server {
namespace {
using privacy_sandbox::server_common::KeyFetcherManagerInterface;

absl::Status InitializeUdfHooksInternal(
    std::function<std::unique_ptr<Lookup>()> get_lookup,
    GetValuesHook& string_get_values_hook,
    GetValuesHook& binary_get_values_hook,
    RunSetQueryStringHook& run_query_hook,
    RunSetQueryIntHook& run_set_query_int_hook,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  PS_VLOG(9, log_context) << "Finishing getValues init";
  string_get_values_hook.FinishInit(get_lookup());
  PS_VLOG(9, log_context) << "Finishing getValuesBinary init";
  binary_get_values_hook.FinishInit(get_lookup());
  PS_VLOG(9, log_context) << "Finishing runQuery init";
  run_query_hook.FinishInit(get_lookup());
  PS_VLOG(9, log_context) << "Finishing runSetQueryInt init";
  run_set_query_int_hook.FinishInit(get_lookup());
  return absl::OkStatus();
}

class NonshardedServerInitializer : public ServerInitializer {
 public:
  explicit NonshardedServerInitializer(
      Cache& cache,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : cache_(cache), log_context_(log_context) {}

  RemoteLookup CreateAndStartRemoteLookupServer() override {
    RemoteLookup remote_lookup;
    return remote_lookup;
  }

  absl::StatusOr<ShardManagerState> InitializeUdfHooks(
      GetValuesHook& string_get_values_hook,
      GetValuesHook& binary_get_values_hook,
      RunSetQueryStringHook& run_query_hook,
      RunSetQueryIntHook& run_set_query_int_hook) override {
    ShardManagerState shard_manager_state;
    auto lookup_supplier = [&cache = cache_]() {
      return CreateLocalLookup(cache);
    };
    InitializeUdfHooksInternal(std::move(lookup_supplier),
                               string_get_values_hook, binary_get_values_hook,
                               run_query_hook, run_set_query_int_hook,
                               log_context_);
    return shard_manager_state;
  }

 private:
  Cache& cache_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

class ShardedServerInitializer : public ServerInitializer {
 public:
  explicit ShardedServerInitializer(
      KeyFetcherManagerInterface& key_fetcher_manager, Lookup& local_lookup,
      std::string environment, int32_t num_shards, int32_t current_shard_num,
      InstanceClient& instance_client, ParameterFetcher& parameter_fetcher,
      KeySharder key_sharder,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : key_fetcher_manager_(key_fetcher_manager),
        local_lookup_(local_lookup),
        environment_(environment),
        num_shards_(num_shards),
        current_shard_num_(current_shard_num),
        instance_client_(instance_client),
        parameter_fetcher_(parameter_fetcher),
        key_sharder_(std::move(key_sharder)),
        log_context_(log_context) {}

  RemoteLookup CreateAndStartRemoteLookupServer() override {
    RemoteLookup remote_lookup;
    remote_lookup.remote_lookup_service = std::make_unique<LookupServiceImpl>(
        local_lookup_, key_fetcher_manager_);
    grpc::ServerBuilder remote_lookup_server_builder;
    auto remoteLookupServerAddress =
        absl::StrCat(kLocalIp, ":", kRemoteLookupServerPort);
    remote_lookup_server_builder.AddListeningPort(
        remoteLookupServerAddress, grpc::InsecureServerCredentials());
    remote_lookup_server_builder.RegisterService(
        remote_lookup.remote_lookup_service.get());
    PS_LOG(INFO, log_context_)
        << "Remote lookup server listening on " << remoteLookupServerAddress;
    remote_lookup.remote_lookup_server =
        remote_lookup_server_builder.BuildAndStart();
    return remote_lookup;
  }

  absl::StatusOr<ShardManagerState> InitializeUdfHooks(
      GetValuesHook& string_get_values_hook,
      GetValuesHook& binary_get_values_hook,
      RunSetQueryStringHook& run_set_query_string_hook,
      RunSetQueryIntHook& run_set_query_int_hook) override {
    auto maybe_shard_state = CreateShardManager();
    if (!maybe_shard_state.ok()) {
      return maybe_shard_state.status();
    }
    auto lookup_supplier = [&local_lookup = local_lookup_,
                            num_shards = num_shards_,
                            current_shard_num = current_shard_num_,
                            &shard_manager = *maybe_shard_state->shard_manager,
                            &key_sharder = key_sharder_]() {
      return CreateShardedLookup(local_lookup, num_shards, current_shard_num,
                                 shard_manager, key_sharder);
    };
    InitializeUdfHooksInternal(std::move(lookup_supplier),
                               string_get_values_hook, binary_get_values_hook,
                               run_set_query_string_hook,
                               run_set_query_int_hook, log_context_);
    return std::move(*maybe_shard_state);
  }

 private:
  absl::StatusOr<ShardManagerState> CreateShardManager() {
    ShardManagerState shard_manager_state;
    PS_VLOG(10, log_context_) << "Creating shard manager";
    shard_manager_state.cluster_mappings_manager =
        ClusterMappingsManager::Create(environment_, num_shards_,
                                       instance_client_, parameter_fetcher_,
                                       log_context_);
    shard_manager_state.shard_manager = TraceRetryUntilOk(
        [&cluster_mappings_manager =
             *shard_manager_state.cluster_mappings_manager,
         &num_shards = num_shards_, &key_fetcher_manager = key_fetcher_manager_,
         &log_context = log_context_] {
          // It might be that the cluster mappings that are passed don't pass
          // validation. E.g. a particular cluster might not have any
          // replicas
          // specified. In that case, we need to retry the creation. After an
          // exponential backoff, that will trigger`GetClusterMappings` which
          // at that point in time might have new replicas spun up.
          return ShardManager::Create(
              num_shards, key_fetcher_manager,
              cluster_mappings_manager.GetClusterMappings(), log_context);
        },
        "GetShardManager", LogStatusSafeMetricsFn<kGetShardManagerStatus>(),
        log_context_);
    auto start_status = shard_manager_state.cluster_mappings_manager->Start(
        *shard_manager_state.shard_manager);
    if (!start_status.ok()) {
      return start_status;
    }
    return std::move(shard_manager_state);
  }
  KeyFetcherManagerInterface& key_fetcher_manager_;
  Lookup& local_lookup_;
  std::string environment_;
  int32_t num_shards_;
  int32_t current_shard_num_;
  InstanceClient& instance_client_;
  ParameterFetcher& parameter_fetcher_;
  KeySharder key_sharder_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

}  // namespace

std::unique_ptr<ServerInitializer> GetServerInitializer(
    int64_t num_shards, KeyFetcherManagerInterface& key_fetcher_manager,
    Lookup& local_lookup, std::string environment, int32_t current_shard_num,
    InstanceClient& instance_client, Cache& cache,
    ParameterFetcher& parameter_fetcher, KeySharder key_sharder,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  CHECK_GT(num_shards, 0) << "num_shards must be greater than 0";
  if (num_shards == 1) {
    return std::make_unique<NonshardedServerInitializer>(cache, log_context);
  }

  return std::make_unique<ShardedServerInitializer>(
      key_fetcher_manager, local_lookup, environment, num_shards,
      current_shard_num, instance_client, parameter_fetcher,
      std::move(key_sharder), log_context);
}
}  // namespace kv_server
