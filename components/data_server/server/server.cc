// Copyright 2022 Google LLC
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

#include "components/data_server/server/server.h"

#include <optional>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "components/data_server/request_handler/get_values_adapter.h"
#include "components/data_server/request_handler/get_values_handler.h"
#include "components/data_server/request_handler/get_values_v2_handler.h"
#include "components/data_server/server/key_fetcher_factory.h"
#include "components/data_server/server/key_value_service_impl.h"
#include "components/data_server/server/key_value_service_v2_impl.h"
#include "components/errors/retry.h"
#include "components/internal_server/constants.h"
#include "components/internal_server/lookup_client.h"
#include "components/internal_server/lookup_server_impl.h"
#include "components/internal_server/run_query_client.h"
#include "components/internal_server/sharded_lookup_server_impl.h"
#include "components/sharding/cluster_mappings_manager.h"
#include "components/telemetry/kv_telemetry.h"
#include "components/udf/hooks/get_values_hook.h"
#include "components/udf/hooks/run_query_hook.h"
#include "components/udf/udf_config_builder.h"
#include "components/util/build_info.h"
#include "glog/logging.h"
#include "grpcpp/ext/proto_server_reflection_plugin.h"
#include "grpcpp/health_check_service_interface.h"
#include "public/constants.h"
#include "public/data_loading/readers/riegeli_stream_io.h"
#include "public/udf/constants.h"
#include "src/cpp/telemetry/init.h"
#include "src/cpp/telemetry/telemetry.h"
#include "src/cpp/telemetry/telemetry_provider.h"
#include "src/google/protobuf/struct.pb.h"

ABSL_FLAG(uint16_t, port, 50051,
          "Port the server is listening on. Defaults to 50051.");
ABSL_FLAG(std::string, internal_server_address, "0.0.0.0:50099",
          "Internal server address. Defaults to 0.0.0.0:50099.");

namespace kv_server {
namespace {

using privacy_sandbox::server_common::ConfigureMetrics;
using privacy_sandbox::server_common::ConfigureTracer;
using privacy_sandbox::server_common::GetTracer;
using privacy_sandbox::server_common::InitTelemetry;
using privacy_sandbox::server_common::TelemetryProvider;

// TODO: Use config cpio client to get this from the environment
constexpr absl::string_view kDataBucketParameterSuffix = "data-bucket-id";
constexpr absl::string_view kBackupPollFrequencySecsParameterSuffix =
    "backup-poll-frequency-secs";
constexpr absl::string_view kMetricsExportIntervalMillisParameterSuffix =
    "metrics-export-interval-millis";
constexpr absl::string_view kMetricsExportTimeoutMillisParameterSuffix =
    "metrics-export-timeout-millis";
constexpr absl::string_view kRealtimeUpdaterThreadNumberParameterSuffix =
    "realtime-updater-num-threads";
constexpr absl::string_view kDataLoadingNumThreadsParameterSuffix =
    "data-loading-num-threads";
constexpr absl::string_view kS3ClientMaxConnectionsParameterSuffix =
    "s3client-max-connections";
constexpr absl::string_view kS3ClientMaxRangeBytesParameterSuffix =
    "s3client-max-range-bytes";
constexpr absl::string_view kNumShardsParameterSuffix = "num-shards";
constexpr absl::string_view kUdfNumWorkersParameterSuffix = "udf-num-workers";
constexpr absl::string_view kRouteV1ToV2Suffix = "route-v1-to-v2";

opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
GetMetricsOptions(const ParameterClient& parameter_client,
                  const std::string environment) {
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
      metrics_options;

  ParameterFetcher parameter_fetcher(environment, parameter_client, nullptr);

  uint32_t export_interval_millis = parameter_fetcher.GetInt32Parameter(
      kMetricsExportIntervalMillisParameterSuffix);
  LOG(INFO) << "Retrieved " << kMetricsExportIntervalMillisParameterSuffix
            << " parameter: " << export_interval_millis;
  uint32_t export_timeout_millis = parameter_fetcher.GetInt32Parameter(
      kMetricsExportTimeoutMillisParameterSuffix);
  LOG(INFO) << "Retrieved " << kMetricsExportTimeoutMillisParameterSuffix
            << " parameter: " << export_timeout_millis;
  metrics_options.export_interval_millis =
      std::chrono::milliseconds(export_interval_millis);
  metrics_options.export_timeout_millis =
      std::chrono::milliseconds(export_timeout_millis);

  return metrics_options;
}

}  // namespace

Server::Server()
    : metrics_recorder_(
          TelemetryProvider::GetInstance().CreateMetricsRecorder()),
      get_values_hook_(GetValuesHook::Create(absl::bind_front(
          LookupClient::Create, absl::GetFlag(FLAGS_internal_server_address)))),
      run_query_hook_(RunQueryHook::Create(
          absl::bind_front(RunQueryClient::Create,
                           absl::GetFlag(FLAGS_internal_server_address)))) {}

// Because the cache relies on metrics_recorder_, this function needs to be
// called right after telemetry has been initialized but before anything that
// requires the cache has been initialized.
void Server::InitializeKeyValueCache() {
  cache_ = KeyValueCache::Create(*metrics_recorder_);
  cache_->UpdateKeyValue(
      "hi",
      "Hello, world! If you are seeing this, it means you can "
      "query me successfully",
      /*logical_commit_time = */ 1);
}

void Server::InitializeTelemetry(const ParameterClient& parameter_client,
                                 InstanceClient& instance_client) {
  std::string instance_id = RetryUntilOk(
      [&instance_client]() { return instance_client.GetInstanceId(); },
      "GetInstanceId", nullptr);

  InitTelemetry(std::string(kServiceName), std::string(BuildVersion()));
  auto metrics_options = GetMetricsOptions(parameter_client, environment_);
  ConfigureMetrics(CreateKVAttributes(instance_id, environment_),
                   metrics_options);
  ConfigureTracer(CreateKVAttributes(std::move(instance_id), environment_));

  metrics_recorder_ = TelemetryProvider::GetInstance().CreateMetricsRecorder();
}

absl::Status Server::CreateDefaultInstancesIfNecessaryAndGetEnvironment(
    std::unique_ptr<const ParameterClient> parameter_client,
    std::unique_ptr<InstanceClient> instance_client,
    std::unique_ptr<UdfClient> udf_client) {
  parameter_client_ = parameter_client == nullptr ? ParameterClient::Create()
                                                  : std::move(parameter_client);
  instance_client_ = instance_client == nullptr
                         ? InstanceClient::Create(*metrics_recorder_)
                         : std::move(instance_client);
  environment_ = TraceRetryUntilOk(
      [this]() { return instance_client_->GetEnvironmentTag(); },
      "GetEnvironment", nullptr);
  LOG(INFO) << "Retrieved environment: " << environment_;
  ParameterFetcher parameter_fetcher(environment_, *parameter_client_,
                                     metrics_recorder_.get());

  int32_t number_of_workers =
      parameter_fetcher.GetInt32Parameter(kUdfNumWorkersParameterSuffix);

  if (udf_client != nullptr) {
    udf_client_ = std::move(udf_client);
    return absl::OkStatus();
  }
  UdfConfigBuilder config_builder;
  // TODO(b/289244673): Once roma interface is updated, internal lookup client
  // can be removed and we can own the unique ptr to the hooks.
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client_or_status =
      UdfClient::Create(config_builder.RegisterGetValuesHook(*get_values_hook_)
                            .RegisterRunQueryHook(*run_query_hook_)
                            .RegisterLoggingHook()
                            .SetNumberOfWorkers(number_of_workers)
                            .Config());
  if (udf_client_or_status.ok()) {
    udf_client_ = std::move(*udf_client_or_status);
  }
  return udf_client_or_status.status();
}

absl::Status Server::Init(
    std::unique_ptr<const ParameterClient> parameter_client,
    std::unique_ptr<InstanceClient> instance_client,
    std::unique_ptr<UdfClient> udf_client) {
  {
    absl::Status status = CreateDefaultInstancesIfNecessaryAndGetEnvironment(
        std::move(parameter_client), std::move(instance_client),
        std::move(udf_client));
    if (!status.ok()) {
      return status;
    }
  }

  // This code is a separate block so that it can't rely on the arguments to
  // this function, which might be nullptr.
  return InitOnceInstancesAreCreated();
}

absl::Status Server::InitOnceInstancesAreCreated() {
  InitializeTelemetry(*parameter_client_, *instance_client_);
  InitializeKeyValueCache();
  auto span = GetTracer()->StartSpan("InitServer");
  auto scope = opentelemetry::trace::Scope(span);
  std::unique_ptr<LifecycleHeartbeat> lifecycle_heartbeat =
      LifecycleHeartbeat::Create(*instance_client_, *metrics_recorder_);
  ParameterFetcher parameter_fetcher(environment_, *parameter_client_,
                                     metrics_recorder_.get());
  if (absl::Status status = lifecycle_heartbeat->Start(parameter_fetcher);
      status != absl::OkStatus()) {
    return status;
  }

  SetDefaultUdfCodeObject();

  const auto shard_num_status = instance_client_->GetShardNumTag();
  if (!shard_num_status.ok()) {
    return shard_num_status.status();
  }
  if (!absl::SimpleAtoi(*shard_num_status, &shard_num_)) {
    std::string error =
        absl::StrFormat("Failed converting shard id parameter: %s to int32.",
                        *shard_num_status);
    LOG(ERROR) << error;
    return absl::InvalidArgumentError(error);
  }
  LOG(INFO) << "Retrieved shard num: " << shard_num_;
  num_shards_ = parameter_fetcher.GetInt32Parameter(kNumShardsParameterSuffix);
  LOG(INFO) << "Retrieved " << kNumShardsParameterSuffix
            << " parameter: " << num_shards_;

  blob_client_ = CreateBlobClient(parameter_fetcher);
  delta_stream_reader_factory_ =
      CreateStreamRecordReaderFactory(parameter_fetcher);
  notifier_ = CreateDeltaFileNotifier(parameter_fetcher);

  key_fetcher_manager_ = CreateKeyFetcherManager(parameter_fetcher);

  CreateGrpcServices(parameter_fetcher);
  auto metadata = parameter_fetcher.GetBlobStorageNotifierMetadata();
  auto message_service_status = MessageService::Create(metadata);
  if (!message_service_status.ok()) {
    return message_service_status.status();
  }
  message_service_blob_ = std::move(*message_service_status);
  SetQueueManager(metadata, message_service_blob_.get());

  grpc_server_ = CreateAndStartGrpcServer();
  remote_lookup_server_ = CreateAndStartRemoteLookupServer();
  {
    auto status_or_notifier = BlobStorageChangeNotifier::Create(
        std::move(metadata), *metrics_recorder_);
    if (!status_or_notifier.ok()) {
      // The ChangeNotifier is required to read delta files, if it's not
      // available that's a critical error and so return immediately.
      return status_or_notifier.status();
    }
    change_notifier_ = std::move(*status_or_notifier);
  }
  auto realtime_notifier_metadata =
      parameter_fetcher.GetRealtimeNotifierMetadata();
  auto realtime_message_service_status = MessageService::Create(
      realtime_notifier_metadata,
      (num_shards_ > 1 ? std::optional<int32_t>(shard_num_) : std::nullopt));
  if (!realtime_message_service_status.ok()) {
    return realtime_message_service_status.status();
  }
  message_service_realtime_ = std::move(*realtime_message_service_status);
  SetQueueManager(realtime_notifier_metadata, message_service_realtime_.get());
  uint32_t realtime_thread_numbers = parameter_fetcher.GetInt32Parameter(
      kRealtimeUpdaterThreadNumberParameterSuffix);
  LOG(INFO) << "Retrieved " << kRealtimeUpdaterThreadNumberParameterSuffix
            << " parameter: " << realtime_thread_numbers;
  for (int i = 0; i < realtime_thread_numbers; i++) {
    auto status_or_notifier =
        ChangeNotifier::Create(realtime_notifier_metadata, *metrics_recorder_);
    if (!status_or_notifier.ok()) {
      return status_or_notifier.status();
    }
    DataOrchestrator::RealtimeOptions realtime_options;
    realtime_options.delta_file_record_change_notifier =
        DeltaFileRecordChangeNotifier::Create(std::move(*status_or_notifier),
                                              *metrics_recorder_);
    realtime_options.realtime_notifier =
        RealtimeNotifier::Create(*metrics_recorder_);
    realtime_options_.push_back(std::move(realtime_options));
  }
  data_orchestrator_ = CreateDataOrchestrator(parameter_fetcher);
  TraceRetryUntilOk([this] { return data_orchestrator_->Start(); },
                    "StartDataOrchestrator", metrics_recorder_.get());
  if (num_shards_ > 1) {
    // At this point the server is healthy and the initialization is over.
    // The only missing piece is having a shard map, which is dependent on
    // other instances being `healthy`. Mark this instance as healthy so that
    // other instances can pull it in for their mapping.
    lifecycle_heartbeat->Finish();
  }
  absl::StatusOr<std::unique_ptr<grpc::Server>> lookup_server_or =
      CreateAndStartInternalLookupServer();
  if (!lookup_server_or.ok()) {
    return lookup_server_or.status();
  }
  internal_lookup_server_ = std::move(*lookup_server_or);
  return absl::OkStatus();
}

void Server::Wait() {
  if (grpc_server_) {
    grpc_server_->Wait();
  }
}

absl::Status Server::MaybeShutdownNotifiers() {
  absl::Status status = absl::OkStatus();
  if (notifier_ && notifier_->IsRunning()) {
    status = notifier_->Stop();
  }
  for (auto& option : realtime_options_) {
    if (option.realtime_notifier->IsRunning()) {
      status.Update(option.realtime_notifier->Stop());
    }
  }
  return status;
}

void Server::GracefulShutdown(absl::Duration timeout) {
  LOG(INFO) << "Graceful gRPC server shutdown requested, timeout: " << timeout;
  if (internal_lookup_server_) {
    internal_lookup_server_->Shutdown();
  }
  if (remote_lookup_server_) {
    remote_lookup_server_->Shutdown();
  }
  if (grpc_server_) {
    grpc_server_->Shutdown(absl::ToChronoTime(absl::Now() + timeout));
  } else {
    LOG(WARNING) << "Server was not started, cannot shut down.";
  }
  if (udf_client_) {
    const absl::Status status = udf_client_->Stop();
    if (!status.ok()) {
      LOG(ERROR) << "Failed to stop UDF client: " << status;
    }
  }
  if (cluster_mappings_manager_ && cluster_mappings_manager_->IsRunning()) {
    const absl::Status status = cluster_mappings_manager_->Stop();
    if (!status.ok()) {
      LOG(ERROR) << "Failed to stop cluster mappings manager: " << status;
    }
  }
  const absl::Status status = MaybeShutdownNotifiers();
  if (!status.ok()) {
    LOG(ERROR) << "Failed to shutdown notifiers.  Got status " << status;
  }
}

void Server::ForceShutdown() {
  LOG(WARNING) << "Immediate gRPC server shutdown requested";
  if (internal_lookup_server_) {
    internal_lookup_server_->Shutdown();
  }
  if (remote_lookup_server_) {
    remote_lookup_server_->Shutdown();
  }
  if (grpc_server_) {
    grpc_server_->Shutdown();
  } else {
    LOG(WARNING) << "Server was not started, cannot shut down.";
  }
  const absl::Status status = MaybeShutdownNotifiers();
  if (!status.ok()) {
    LOG(ERROR) << "Failed to shutdown notifiers.  Got status " << status;
  }
  if (udf_client_) {
    const absl::Status status = udf_client_->Stop();
    if (!status.ok()) {
      LOG(ERROR) << "Failed to stop UDF client: " << status;
    }
  }
  if (cluster_mappings_manager_ && cluster_mappings_manager_->IsRunning()) {
    const absl::Status status = cluster_mappings_manager_->Stop();
    if (!status.ok()) {
      LOG(ERROR) << "Failed to stop cluster mappings manager: " << status;
    }
  }
}

std::unique_ptr<BlobStorageClient> Server::CreateBlobClient(
    const ParameterFetcher& parameter_fetcher) {
  const int32_t s3client_max_connections = parameter_fetcher.GetInt32Parameter(
      kS3ClientMaxConnectionsParameterSuffix);
  LOG(INFO) << "Retrieved " << kS3ClientMaxConnectionsParameterSuffix
            << " parameter: " << s3client_max_connections;
  const int32_t s3client_max_range_bytes = parameter_fetcher.GetInt32Parameter(
      kS3ClientMaxRangeBytesParameterSuffix);
  LOG(INFO) << "Retrieved " << kS3ClientMaxRangeBytesParameterSuffix
            << " parameter: " << s3client_max_range_bytes;
  BlobStorageClient::ClientOptions options;
  options.max_connections = s3client_max_connections;
  options.max_range_bytes = s3client_max_range_bytes;
  return BlobStorageClient::Create(*metrics_recorder_, options);
}

std::unique_ptr<StreamRecordReaderFactory<std::string_view>>
Server::CreateStreamRecordReaderFactory(
    const ParameterFetcher& parameter_fetcher) {
  const int32_t data_loading_num_threads = parameter_fetcher.GetInt32Parameter(
      kDataLoadingNumThreadsParameterSuffix);
  LOG(INFO) << "Retrieved " << kDataLoadingNumThreadsParameterSuffix
            << " parameter: " << data_loading_num_threads;
  ConcurrentStreamRecordReader<std::string_view>::Options options;
  options.num_worker_threads = data_loading_num_threads;
  return StreamRecordReaderFactory<std::string_view>::Create(options);
}

std::unique_ptr<DataOrchestrator> Server::CreateDataOrchestrator(
    const ParameterFetcher& parameter_fetcher) {
  const std::string data_bucket =
      parameter_fetcher.GetParameter(kDataBucketParameterSuffix);
  LOG(INFO) << "Retrieved " << kDataBucketParameterSuffix
            << " parameter: " << data_bucket;
  return TraceRetryUntilOk(
      [&] {
        return DataOrchestrator::TryCreate(
            {
                .data_bucket = data_bucket,
                .cache = *cache_,
                .blob_client = *blob_client_,
                .delta_notifier = *notifier_,
                .change_notifier = *change_notifier_,
                .delta_stream_reader_factory = *delta_stream_reader_factory_,
                .realtime_options = realtime_options_,
                .udf_client = *udf_client_,
                .shard_num = shard_num_,
                .num_shards = num_shards_,
            },
            *metrics_recorder_);
      },
      "CreateDataOrchestrator", metrics_recorder_.get());
}

void Server::CreateGrpcServices(const ParameterFetcher& parameter_fetcher) {
  const bool use_v2 = parameter_fetcher.GetBoolParameter(kRouteV1ToV2Suffix);
  LOG(INFO) << "Retrieved " << kRouteV1ToV2Suffix << " parameter: " << use_v2;
  get_values_adapter_ =
      GetValuesAdapter::Create(std::make_unique<GetValuesV2Handler>(
          *udf_client_, *metrics_recorder_, *key_fetcher_manager_));
  GetValuesHandler handler(*cache_, *get_values_adapter_, *metrics_recorder_,
                           use_v2);
  grpc_services_.push_back(std::make_unique<KeyValueServiceImpl>(
      std::move(handler), *metrics_recorder_));
  GetValuesV2Handler v2handler(*udf_client_, *metrics_recorder_,
                               *key_fetcher_manager_);
  grpc_services_.push_back(std::make_unique<KeyValueServiceV2Impl>(
      std::move(v2handler), *metrics_recorder_));
}

std::unique_ptr<grpc::Server> Server::CreateAndStartGrpcServer() {
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  grpc::ServerBuilder builder;
  const std::string server_address =
      absl::StrCat("0.0.0.0:", absl::GetFlag(FLAGS_port));
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to a *synchronous* service.
  for (auto& service : grpc_services_) {
    builder.RegisterService(service.get());
  }
  // Finally assemble the server.
  LOG(INFO) << "Server listening on " << server_address << std::endl;
  return builder.BuildAndStart();
}

absl::Status Server::CreateShardManager() {
  cluster_mappings_manager_ = std::make_unique<ClusterMappingsManager>(
      environment_, num_shards_, *metrics_recorder_, *instance_client_);
  shard_manager_ = TraceRetryUntilOk(
      [&cluster_mappings_manager = *cluster_mappings_manager_,
       &num_shards = num_shards_, &key_fetcher_manager = *key_fetcher_manager_,
       &metrics_recorder = *metrics_recorder_] {
        // It might be that the cluster mappings that are passed don't pass
        // validation. E.g. a particular cluster might not have any replicas
        // specified. In that case, we need to retry the creation. After an
        // exponential backoff, that will trigger`GetClusterMappings` which
        // at that point in time might have new replicas spun up.
        return ShardManager::Create(
            num_shards, key_fetcher_manager,
            cluster_mappings_manager.GetClusterMappings(), metrics_recorder);
      },
      "GetShardManager", metrics_recorder_.get());
  return cluster_mappings_manager_->Start(*shard_manager_);
}

absl::StatusOr<std::unique_ptr<grpc::Server>>
Server::CreateAndStartInternalLookupServer() {
  if (num_shards_ <= 1) {
    internal_lookup_service_ = std::make_unique<LookupServiceImpl>(
        *cache_, *key_fetcher_manager_, *metrics_recorder_);
  } else {
    if (const absl::Status status = CreateShardManager(); !status.ok()) {
      return status;
    }
    internal_lookup_service_ = std::make_unique<ShardedLookupServiceImpl>(
        *metrics_recorder_, *cache_, num_shards_, shard_num_, *shard_manager_);
  }

  grpc::ServerBuilder internal_lookup_server_builder;
  const std::string internal_server_address =
      absl::GetFlag(FLAGS_internal_server_address);
  internal_lookup_server_builder.AddListeningPort(
      internal_server_address, grpc::InsecureServerCredentials());
  internal_lookup_server_builder.RegisterService(
      internal_lookup_service_.get());

  LOG(INFO) << "Internal lookup server listening on " << internal_server_address
            << std::endl;
  return internal_lookup_server_builder.BuildAndStart();
}

std::unique_ptr<grpc::Server> Server::CreateAndStartRemoteLookupServer() {
  if (num_shards_ <= 1) {
    return nullptr;
  }

  remote_lookup_service_ = std::make_unique<LookupServiceImpl>(
      *cache_, *key_fetcher_manager_, *metrics_recorder_);
  grpc::ServerBuilder remote_lookup_server_builder;
  auto remoteLookupServerAddress =
      absl::StrCat(kLocalIp, ":", kRemoteLookupServerPort);
  remote_lookup_server_builder.AddListeningPort(
      remoteLookupServerAddress, grpc::InsecureServerCredentials());
  remote_lookup_server_builder.RegisterService(remote_lookup_service_.get());
  LOG(INFO) << "Remote lookup server listening on " << remoteLookupServerAddress
            << std::endl;
  return remote_lookup_server_builder.BuildAndStart();
}

void Server::SetDefaultUdfCodeObject() {
  const absl::Status status = udf_client_->SetCodeObject(
      CodeConfig{.js = kDefaultUdfCodeSnippet,
                 .udf_handler_name = kDefaultUdfHandlerName,
                 .logical_commit_time = kDefaultLogicalCommitTime,
                 .version = kDefaultVersion});
  if (!status.ok()) {
    LOG(ERROR) << "Error setting code object: " << status;
  }
}

std::unique_ptr<DeltaFileNotifier> Server::CreateDeltaFileNotifier(
    const ParameterFetcher& parameter_fetcher) {
  uint32_t backup_poll_frequency_secs = parameter_fetcher.GetInt32Parameter(
      kBackupPollFrequencySecsParameterSuffix);
  LOG(INFO) << "Retrieved " << kBackupPollFrequencySecsParameterSuffix
            << " parameter: " << backup_poll_frequency_secs;

  return DeltaFileNotifier::Create(*blob_client_,
                                   absl::Seconds(backup_poll_frequency_secs));
}

}  // namespace kv_server
