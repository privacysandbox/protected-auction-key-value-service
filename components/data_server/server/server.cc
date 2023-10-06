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
#include "components/internal_server/local_lookup.h"
#include "components/internal_server/lookup_server_impl.h"
#include "components/internal_server/sharded_lookup.h"
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
constexpr absl::string_view kUseExternalMetricsCollectorEndpointSuffix =
    "use-external-metrics-collector-endpoint";
constexpr absl::string_view kMetricsCollectorEndpointSuffix =
    "metrics-collector-endpoint";
constexpr absl::string_view kMetricsExportIntervalMillisParameterSuffix =
    "metrics-export-interval-millis";
constexpr absl::string_view kMetricsExportTimeoutMillisParameterSuffix =
    "metrics-export-timeout-millis";
constexpr absl::string_view kRealtimeUpdaterThreadNumberParameterSuffix =
    "realtime-updater-num-threads";
constexpr absl::string_view kDataLoadingNumThreadsParameterSuffix =
    "data-loading-num-threads";
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

absl::Status CheckMetricsCollectorEndPointConnection(
    std::string_view collector_endpoint) {
  auto channel = grpc::CreateChannel(std::string(collector_endpoint),
                                     grpc::InsecureChannelCredentials());
  // TODO(b/300137699): make the connection timeout a parameter
  if (!channel->WaitForConnected(
          gpr_time_add(gpr_now(GPR_CLOCK_REALTIME),
                       gpr_time_from_seconds(120, GPR_TIMESPAN)))) {
    return absl::DeadlineExceededError(
        "Timeout waiting for metrics collector connection");
  }
  LOG(INFO) << "Metrics collector is connected";
  return absl::OkStatus();
}

absl::optional<std::string> GetMetricsCollectorEndPoint(
    const ParameterClient& parameter_client, const std::string& environment) {
  absl::optional<std::string> metrics_collection_endpoint;
  ParameterFetcher parameter_fetcher(environment, parameter_client, nullptr);
  auto should_connect_to_external_metrics_collector =
      parameter_fetcher.GetBoolParameter(
          kUseExternalMetricsCollectorEndpointSuffix);
  if (should_connect_to_external_metrics_collector) {
    std::string metrics_collector_endpoint_value =
        parameter_fetcher.GetParameter(kMetricsCollectorEndpointSuffix);
    LOG(INFO) << "Retrieved " << kMetricsCollectorEndpointSuffix
              << " parameter: " << metrics_collector_endpoint_value;
    metrics_collection_endpoint = std::move(metrics_collector_endpoint_value);
  }
  return std::move(metrics_collection_endpoint);
}

}  // namespace

Server::Server()
    : metrics_recorder_(
          TelemetryProvider::GetInstance().CreateMetricsRecorder()),
      string_get_values_hook_(
          GetValuesHook::Create(GetValuesHook::OutputType::kString)),
      binary_get_values_hook_(
          GetValuesHook::Create(GetValuesHook::OutputType::kBinary)),
      run_query_hook_(RunQueryHook::Create()) {}

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
  auto metrics_collector_endpoint =
      GetMetricsCollectorEndPoint(parameter_client, environment_);
  if (metrics_collector_endpoint.has_value()) {
    if (const absl::Status status = CheckMetricsCollectorEndPointConnection(
            metrics_collector_endpoint.value());
        !status.ok()) {
      LOG(ERROR) << "Error in connecting metrics collector: " << status;
    }
  }
  ConfigureMetrics(CreateKVAttributes(instance_id, environment_),
                   metrics_options, metrics_collector_endpoint);
  ConfigureTracer(CreateKVAttributes(std::move(instance_id), environment_),
                  metrics_collector_endpoint);

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
      UdfClient::Create(
          config_builder.RegisterStringGetValuesHook(*string_get_values_hook_)
              .RegisterBinaryGetValuesHook(*binary_get_values_hook_)
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
  metrics_recorder_->SetCommonLabel("shard_number", std::to_string(shard_num_));
  num_shards_ = parameter_fetcher.GetInt32Parameter(kNumShardsParameterSuffix);
  LOG(INFO) << "Retrieved " << kNumShardsParameterSuffix
            << " parameter: " << num_shards_;

  blob_client_ = CreateBlobClient(parameter_fetcher);
  delta_stream_reader_factory_ =
      CreateStreamRecordReaderFactory(parameter_fetcher);
  notifier_ = CreateDeltaFileNotifier(parameter_fetcher);
  auto factory = KeyFetcherFactory::Create();
  key_fetcher_manager_ = factory->CreateKeyFetcherManager(parameter_fetcher);
  CreateGrpcServices(parameter_fetcher);
  auto metadata = parameter_fetcher.GetBlobStorageNotifierMetadata();
  auto message_service_status = MessageService::Create(metadata);
  if (!message_service_status.ok()) {
    return message_service_status.status();
  }
  message_service_blob_ = std::move(*message_service_status);
  SetQueueManager(metadata, message_service_blob_.get());

  grpc_server_ = CreateAndStartGrpcServer();
  local_lookup_ = CreateLocalLookup(*cache_, *metrics_recorder_);
  auto server_initializer = GetServerInitializer(
      num_shards_, *metrics_recorder_, *key_fetcher_manager_, *local_lookup_,
      environment_, shard_num_, *instance_client_, *cache_);
  remote_lookup_ = server_initializer->CreateAndStartRemoteLookupServer();
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
      parameter_fetcher.GetRealtimeNotifierMetadata(num_shards_, shard_num_);
  auto realtime_message_service_status =
      MessageService::Create(realtime_notifier_metadata);
  if (!realtime_message_service_status.ok()) {
    return realtime_message_service_status.status();
  }
  message_service_realtime_ = std::move(*realtime_message_service_status);
  SetQueueManager(realtime_notifier_metadata, message_service_realtime_.get());
  uint32_t realtime_thread_numbers = parameter_fetcher.GetInt32Parameter(
      kRealtimeUpdaterThreadNumberParameterSuffix);
  LOG(INFO) << "Retrieved " << kRealtimeUpdaterThreadNumberParameterSuffix
            << " parameter: " << realtime_thread_numbers;
  auto maybe_realtime_thread_pool_manager = RealtimeThreadPoolManager::Create(
      *metrics_recorder_, realtime_notifier_metadata, realtime_thread_numbers);
  if (!maybe_realtime_thread_pool_manager.ok()) {
    return maybe_realtime_thread_pool_manager.status();
  }
  realtime_thread_pool_manager_ =
      std::move(*maybe_realtime_thread_pool_manager);
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
  auto maybe_shard_state = server_initializer->InitializeUdfHooks(
      *string_get_values_hook_, *binary_get_values_hook_, *run_query_hook_);
  if (!maybe_shard_state.ok()) {
    return maybe_shard_state.status();
  }
  shard_manager_state_ = *std::move(maybe_shard_state);
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
  if (realtime_thread_pool_manager_) {
    status.Update(realtime_thread_pool_manager_->Stop());
  }
  return status;
}

void Server::GracefulShutdown(absl::Duration timeout) {
  LOG(INFO) << "Graceful gRPC server shutdown requested, timeout: " << timeout;
  if (internal_lookup_server_) {
    internal_lookup_server_->Shutdown();
  }
  if (remote_lookup_.remote_lookup_server) {
    remote_lookup_.remote_lookup_server->Shutdown();
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
  if (shard_manager_state_.cluster_mappings_manager &&
      shard_manager_state_.cluster_mappings_manager->IsRunning()) {
    const absl::Status status =
        shard_manager_state_.cluster_mappings_manager->Stop();
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
  if (remote_lookup_.remote_lookup_server) {
    remote_lookup_.remote_lookup_server->Shutdown();
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
  if (shard_manager_state_.cluster_mappings_manager &&
      shard_manager_state_.cluster_mappings_manager->IsRunning()) {
    const absl::Status status =
        shard_manager_state_.cluster_mappings_manager->Stop();
    if (!status.ok()) {
      LOG(ERROR) << "Failed to stop cluster mappings manager: " << status;
    }
  }
}

std::unique_ptr<BlobStorageClient> Server::CreateBlobClient(
    const ParameterFetcher& parameter_fetcher) {
  BlobStorageClient::ClientOptions client_options =
      parameter_fetcher.GetBlobStorageClientOptions();
  std::unique_ptr<BlobStorageClientFactory> blob_storage_client_factory =
      BlobStorageClientFactory::Create();
  return blob_storage_client_factory->CreateBlobStorageClient(
      *metrics_recorder_, std::move(client_options));
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
                .realtime_thread_pool_manager = *realtime_thread_pool_manager_,
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
