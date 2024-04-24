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

#ifndef COMPONENTS_DATA_SERVER_SERVER_SERVER_H_
#define COMPONENTS_DATA_SERVER_SERVER_SERVER_H_

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "components/cloud_config/instance_client.h"
#include "components/cloud_config/parameter_client.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/blob_storage/delta_file_notifier.h"
#include "components/data/realtime/realtime_thread_pool_manager.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/key_value_cache.h"
#include "components/data_server/data_loading/data_orchestrator.h"
#include "components/data_server/request_handler/get_values_adapter.h"
#include "components/data_server/server/lifecycle_heartbeat.h"
#include "components/data_server/server/parameter_fetcher.h"
#include "components/data_server/server/server_initializer.h"
#include "components/internal_server/lookup.h"
#include "components/sharding/cluster_mappings_manager.h"
#include "components/sharding/shard_manager.h"
#include "components/telemetry/open_telemetry_sink.h"
#include "components/udf/hooks/get_values_hook.h"
#include "components/udf/hooks/run_query_hook.h"
#include "components/udf/udf_client.h"
#include "components/util/platform_initializer.h"
#include "components/util/safe_path_log_context.h"
#include "grpcpp/grpcpp.h"
#include "public/base_types.pb.h"
#include "public/query/get_values.grpc.pb.h"
#include "public/sharding/key_sharder.h"
#include "src/telemetry/telemetry.h"

namespace kv_server {

class Server {
 public:
  Server();

  // Arguments that are nullptr will be created, they may be passed in for
  // unit testing purposes.
  absl::Status Init(std::unique_ptr<ParameterClient> parameter_client = nullptr,
                    std::unique_ptr<InstanceClient> instance_client = nullptr,
                    std::unique_ptr<UdfClient> udf_client = nullptr);

  // Wait for the server to shut down. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  void Wait();

  // Stop the server either gracefully, that may fail after a timeout, or
  // immediately.
  void GracefulShutdown(absl::Duration timeout);
  void ForceShutdown();

 private:
  // If objects were not passed in for unit testing purposes then create them.
  absl::Status CreateDefaultInstancesIfNecessaryAndGetEnvironment(
      std::unique_ptr<ParameterClient> parameter_client,
      std::unique_ptr<InstanceClient> instance_client,
      std::unique_ptr<UdfClient> udf_client);

  absl::Status InitOnceInstancesAreCreated();
  void InitializeKeyValueCache();

  std::unique_ptr<BlobStorageClient> CreateBlobClient(
      const ParameterFetcher& parameter_fetcher);
  std::unique_ptr<StreamRecordReaderFactory> CreateStreamRecordReaderFactory(
      const ParameterFetcher& parameter_fetcher);
  std::unique_ptr<DataOrchestrator> CreateDataOrchestrator(
      const ParameterFetcher& parameter_fetcher, KeySharder key_sharder);

  void CreateGrpcServices(const ParameterFetcher& parameter_fetcher);
  absl::Status MaybeShutdownNotifiers();

  std::unique_ptr<grpc::Server> CreateAndStartGrpcServer();

  std::unique_ptr<DeltaFileNotifier> CreateDeltaFileNotifier(
      const ParameterFetcher& parameter_fetcher);

  absl::Status InitializeUdfHooks();
  std::unique_ptr<grpc::Server> CreateAndStartRemoteLookupServer();

  absl::Status SetDefaultUdfCodeObject();

  void InitializeTelemetry(const ParameterClient& parameter_client,
                           InstanceClient& instance_client);
  absl::Status CreateShardManager();
  void InitLogger(::opentelemetry::sdk::resource::Resource server_info,
                  absl::optional<std::string> collector_endpoint,
                  const ParameterFetcher& parameter_fetcher);

  // This must be first, otherwise the AWS SDK will crash when it's called:
  PlatformInitializer platform_initializer_;

  std::unique_ptr<ParameterClient> parameter_client_;
  std::unique_ptr<InstanceClient> instance_client_;
  std::string environment_;
  std::vector<std::unique_ptr<grpc::Service>> grpc_services_;
  std::unique_ptr<grpc::Server> grpc_server_;
  std::unique_ptr<Cache> cache_;
  std::unique_ptr<GetValuesAdapter> get_values_adapter_;
  std::unique_ptr<GetValuesHook> string_get_values_hook_;
  std::unique_ptr<GetValuesHook> binary_get_values_hook_;
  std::unique_ptr<RunSetQueryIntHook> run_set_query_int_hook_;
  std::unique_ptr<RunSetQueryStringHook> run_set_query_string_hook_;

  // BlobStorageClient must outlive DeltaFileNotifier
  std::unique_ptr<BlobStorageClient> blob_client_;

  std::unique_ptr<MessageService> message_service_blob_;
  std::unique_ptr<MessageService> message_service_realtime_;

  // The following fields must outlive DataOrchestrator
  std::unique_ptr<DeltaFileNotifier> notifier_;
  std::unique_ptr<BlobStorageChangeNotifier> change_notifier_;
  std::unique_ptr<RealtimeThreadPoolManager> realtime_thread_pool_manager_;
  std::unique_ptr<StreamRecordReaderFactory> delta_stream_reader_factory_;

  std::unique_ptr<DataOrchestrator> data_orchestrator_;

  // Helper for lookup.proto calls that reads from local cache only
  std::unique_ptr<Lookup> local_lookup_;
  // Helper for lookup.proto calls that reads from shards
  std::unique_ptr<Lookup> sharded_lookup_;

  // Internal Lookup Server -- lookup requests to this server originate (from
  // UDF sandbox) and terminate on the same machine.
  std::unique_ptr<grpc::Service> internal_lookup_service_;
  std::unique_ptr<grpc::Server> internal_lookup_server_;

  RemoteLookup remote_lookup_;
  std::unique_ptr<UdfClient> udf_client_;
  ShardManagerState shard_manager_state_;

  int32_t shard_num_;
  int32_t num_shards_;

  std::unique_ptr<privacy_sandbox::server_common::KeyFetcherManagerInterface>
      key_fetcher_manager_;
  std::unique_ptr<OpenTelemetrySink> open_telemetry_sink_;
  KVServerSafeLogContext server_safe_log_context_;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_SERVER_SERVER_H_
