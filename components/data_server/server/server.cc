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
#include "absl/log/log.h"
#include "absl/log/log_sink_registry.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "components/data/blob_storage/blob_prefix_allowlist.h"
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
#include "components/telemetry/server_definition.h"
#include "components/udf/hooks/get_values_hook.h"
#include "components/udf/hooks/run_query_hook.h"
#include "components/udf/udf_config_builder.h"
#include "components/util/build_info.h"
#include "google/protobuf/text_format.h"
#include "grpcpp/ext/proto_server_reflection_plugin.h"
#include "grpcpp/health_check_service_interface.h"
#include "public/constants.h"
#include "public/data_loading/readers/avro_stream_record_reader_factory.h"
#include "public/data_loading/readers/riegeli_stream_record_reader_factory.h"
#include "public/data_loading/readers/stream_record_reader_factory.h"
#include "public/udf/constants.h"
#include "src/google/protobuf/struct.pb.h"
#include "src/telemetry/init.h"
#include "src/telemetry/telemetry.h"
#include "src/telemetry/telemetry_provider.h"

ABSL_FLAG(uint16_t, port, 50051,
          "Port the server is listening on. Defaults to 50051.");

namespace kv_server {
namespace {

using privacy_sandbox::server_common::ConfigureMetrics;
using privacy_sandbox::server_common::ConfigurePrivateMetrics;
using privacy_sandbox::server_common::ConfigureTracer;
using privacy_sandbox::server_common::GetTracer;
using privacy_sandbox::server_common::InitTelemetry;
using privacy_sandbox::server_common::TelemetryProvider;
using privacy_sandbox::server_common::telemetry::BuildDependentConfig;

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
constexpr absl::string_view kDataLoadingFileFormatSuffix =
    "data-loading-file-format";
constexpr absl::string_view kNumShardsParameterSuffix = "num-shards";
constexpr absl::string_view kUdfNumWorkersParameterSuffix = "udf-num-workers";
constexpr absl::string_view kLoggingVerbosityLevelParameterSuffix =
    "logging-verbosity-level";
constexpr absl::string_view kUdfTimeoutMillisParameterSuffix =
    "udf-timeout-millis";
constexpr absl::string_view kUdfMinLogLevelParameterSuffix =
    "udf-min-log-level";
constexpr absl::string_view kUseShardingKeyRegexParameterSuffix =
    "use-sharding-key-regex";
constexpr absl::string_view kShardingKeyRegexParameterSuffix =
    "sharding-key-regex";
constexpr absl::string_view kRouteV1ToV2Suffix = "route-v1-to-v2";
constexpr absl::string_view kAddMissingKeysV1Suffix = "add-missing-keys-v1";
constexpr absl::string_view kAutoscalerHealthcheck = "autoscaler-healthcheck";
constexpr absl::string_view kLoadbalancerHealthcheck =
    "loadbalancer-healthcheck";
constexpr absl::string_view kEnableOtelLoggerParameterSuffix =
    "enable-otel-logger";
constexpr std::string_view kDataLoadingBlobPrefixAllowlistSuffix =
    "data-loading-blob-prefix-allowlist";
constexpr std::string_view kTelemetryConfigSuffix = "telemetry-config";

opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
GetMetricsOptions(const ParameterClient& parameter_client,
                  const std::string environment) {
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
      metrics_options;

  ParameterFetcher parameter_fetcher(environment, parameter_client);

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

void CheckMetricsCollectorEndPointConnection(
    std::string_view collector_endpoint) {
  auto channel = grpc::CreateChannel(std::string(collector_endpoint),
                                     grpc::InsecureChannelCredentials());
  RetryUntilOk(
      [channel]() {
        if (channel->GetState(true) != GRPC_CHANNEL_READY) {
          return absl::UnavailableError("metrics collector is not connected");
        }
        return absl::OkStatus();
      },
      "Checking connection to metrics collector", LogMetricsNoOpCallback());
}

absl::optional<std::string> GetMetricsCollectorEndPoint(
    const ParameterClient& parameter_client, const std::string& environment) {
  absl::optional<std::string> metrics_collection_endpoint;
  ParameterFetcher parameter_fetcher(environment, parameter_client);
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

privacy_sandbox::server_common::telemetry::TelemetryConfig
GetServerTelemetryConfig(const ParameterClient& parameter_client,
                         const std::string& environment) {
  ParameterFetcher parameter_fetcher(environment, parameter_client);
  auto config_string = parameter_fetcher.GetParameter(kTelemetryConfigSuffix);
  privacy_sandbox::server_common::telemetry::TelemetryConfig config;
  if (!google::protobuf::TextFormat::ParseFromString(config_string, &config)) {
    LOG(ERROR) << "Invalid proto format for telemetry config " << config_string
               << ", fall back to prod config mode";
    config.set_mode(
        privacy_sandbox::server_common::telemetry::TelemetryConfig::PROD);
  }
  return config;
}

BlobPrefixAllowlist GetBlobPrefixAllowlist(
    const ParameterFetcher& parameter_fetcher) {
  const auto prefix_allowlist = parameter_fetcher.GetParameter(
      kDataLoadingBlobPrefixAllowlistSuffix, /*default_value=*/"");
  LOG(INFO) << "Retrieved " << kDataLoadingBlobPrefixAllowlistSuffix
            << " parameter: " << prefix_allowlist;
  return BlobPrefixAllowlist(prefix_allowlist);
}

}  // namespace

Server::Server()
    : string_get_values_hook_(
          GetValuesHook::Create(GetValuesHook::OutputType::kString)),
      binary_get_values_hook_(
          GetValuesHook::Create(GetValuesHook::OutputType::kBinary)),
      run_query_hook_(RunQueryHook::Create()) {}

// Because the cache relies on telemetry, this function needs to be
// called right after telemetry has been initialized but before anything that
// requires the cache has been initialized.
void Server::InitializeKeyValueCache() {
  cache_ = KeyValueCache::Create();
  cache_->UpdateKeyValue(
      log_context_, "hi",
      "Hello, world! If you are seeing this, it means you can "
      "query me successfully",
      /*logical_commit_time = */ 1);
}

void Server::InitOtelLogger(
    ::opentelemetry::sdk::resource::Resource server_info,
    absl::optional<std::string> collector_endpoint,
    const ParameterFetcher& parameter_fetcher) {
  const bool enable_otel_logger =
      parameter_fetcher.GetBoolParameter(kEnableOtelLoggerParameterSuffix);
  LOG(INFO) << "Retrieved " << kEnableOtelLoggerParameterSuffix
            << " parameter: " << enable_otel_logger;
  if (!enable_otel_logger) {
    return;
  }
  log_provider_ = privacy_sandbox::server_common::ConfigurePrivateLogger(
      server_info, collector_endpoint);
  open_telemetry_sink_ = std::make_unique<OpenTelemetrySink>(
      log_provider_->GetLogger(kServiceName.data()));
  absl::AddLogSink(open_telemetry_sink_.get());
}

void Server::InitializeTelemetry(const ParameterClient& parameter_client,
                                 InstanceClient& instance_client) {
  std::string instance_id = RetryUntilOk(
      [&instance_client]() { return instance_client.GetInstanceId(); },
      "GetInstanceId", LogMetricsNoOpCallback());

  InitTelemetry(std::string(kServiceName), std::string(BuildVersion()));
  auto metrics_options = GetMetricsOptions(parameter_client, environment_);
  auto metrics_collector_endpoint =
      GetMetricsCollectorEndPoint(parameter_client, environment_);
  if (metrics_collector_endpoint.has_value()) {
    CheckMetricsCollectorEndPointConnection(metrics_collector_endpoint.value());
  }
  LOG(INFO) << "Done retrieving metrics collector endpoint";
  BuildDependentConfig telemetry_config(
      GetServerTelemetryConfig(parameter_client, environment_));
  auto* context_map = KVServerContextMap(
      telemetry_config,
      ConfigurePrivateMetrics(
          CreateKVAttributes(instance_id, std::to_string(shard_num_),
                             environment_),
          metrics_options, metrics_collector_endpoint));
  AddSystemMetric(context_map);

  auto* internal_lookup_context_map = InternalLookupServerContextMap(
      telemetry_config,
      ConfigurePrivateMetrics(
          CreateKVAttributes(instance_id, std::to_string(shard_num_),
                             environment_),
          metrics_options, metrics_collector_endpoint));

  // TODO(b/300137699): Deprecate ConfigureMetrics once all metrics are migrated
  // to new telemetry API
  ConfigureMetrics(
      CreateKVAttributes(instance_id, std::to_string(shard_num_), environment_),
      metrics_options, metrics_collector_endpoint);
  ConfigureTracer(
      CreateKVAttributes(instance_id, std::to_string(shard_num_), environment_),
      metrics_collector_endpoint);
  ParameterFetcher parameter_fetcher(environment_, parameter_client);
  InitOtelLogger(CreateKVAttributes(std::move(instance_id),
                                    std::to_string(shard_num_), environment_),
                 metrics_collector_endpoint, parameter_fetcher);
  LOG(INFO) << "Done init telemetry";
}

absl::Status Server::CreateDefaultInstancesIfNecessaryAndGetEnvironment(
    std::unique_ptr<const ParameterClient> parameter_client,
    std::unique_ptr<InstanceClient> instance_client,
    std::unique_ptr<UdfClient> udf_client) {
  parameter_client_ = parameter_client == nullptr ? ParameterClient::Create()
                                                  : std::move(parameter_client);
  instance_client_ = instance_client == nullptr ? InstanceClient::Create()
                                                : std::move(instance_client);
  environment_ = TraceRetryUntilOk(
      [this]() { return instance_client_->GetEnvironmentTag(); },
      "GetEnvironment", LogMetricsNoOpCallback());
  LOG(INFO) << "Retrieved environment: " << environment_;
  ParameterFetcher parameter_fetcher(environment_, *parameter_client_);

  int32_t number_of_workers =
      parameter_fetcher.GetInt32Parameter(kUdfNumWorkersParameterSuffix);
  int32_t udf_timeout_ms =
      parameter_fetcher.GetInt32Parameter(kUdfTimeoutMillisParameterSuffix);
  int32_t udf_min_log_level =
      parameter_fetcher.GetInt32Parameter(kUdfMinLogLevelParameterSuffix);

  // updating verbosity level flag as early as we can, as it affects all logging
  // downstream.
  absl::SetGlobalVLogLevel(parameter_fetcher.GetInt32Parameter(
      kLoggingVerbosityLevelParameterSuffix));
  if (udf_client != nullptr) {
    udf_client_ = std::move(udf_client);
    return absl::OkStatus();
  }
  UdfConfigBuilder config_builder;
  // TODO(b/289244673): Once roma interface is updated, internal lookup client
  // can be removed and we can own the unique ptr to the hooks.
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client_or_status =
      UdfClient::Create(
          std::move(config_builder
                        .RegisterStringGetValuesHook(*string_get_values_hook_)
                        .RegisterBinaryGetValuesHook(*binary_get_values_hook_)
                        .RegisterRunQueryHook(*run_query_hook_)
                        .RegisterLoggingFunction()
                        .SetNumberOfWorkers(number_of_workers)
                        .Config()),
          absl::Milliseconds(udf_timeout_ms), udf_min_log_level);
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

KeySharder GetKeySharder(const ParameterFetcher& parameter_fetcher) {
  const bool use_sharding_key_regex =
      parameter_fetcher.GetBoolParameter(kUseShardingKeyRegexParameterSuffix);
  LOG(INFO) << "Retrieved " << kUseShardingKeyRegexParameterSuffix
            << " parameter: " << use_sharding_key_regex;
  ShardingFunction func(/*seed=*/"");
  std::optional<std::regex> shard_key_regex;
  if (use_sharding_key_regex) {
    std::string sharding_key_regex_value =
        parameter_fetcher.GetParameter(kShardingKeyRegexParameterSuffix);
    LOG(INFO) << "Retrieved " << kShardingKeyRegexParameterSuffix
              << " parameter: " << sharding_key_regex_value;
    // https://en.cppreference.com/w/cpp/regex/syntax_option_type
    // optimize -- "Instructs the regular expression engine to make matching
    // faster, with the potential cost of making construction slower. For
    // example, this might mean converting a non-deterministic FSA to a
    // deterministic FSA." this matches our usecase.
    shard_key_regex = std::regex(std::move(sharding_key_regex_value),
                                 std::regex_constants::optimize);
  }
  return KeySharder(func, std::move(shard_key_regex));
}

absl::Status Server::InitOnceInstancesAreCreated() {
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
  InitializeTelemetry(*parameter_client_, *instance_client_);
  InitializeKeyValueCache();
  auto span = GetTracer()->StartSpan("InitServer");
  auto scope = opentelemetry::trace::Scope(span);
  LOG(INFO) << "Creating lifecycle heartbeat...";
  std::unique_ptr<LifecycleHeartbeat> lifecycle_heartbeat =
      LifecycleHeartbeat::Create(*instance_client_);
  ParameterFetcher parameter_fetcher(
      environment_, *parameter_client_,
      std::move(LogStatusSafeMetricsFn<kGetParameterStatus>()));
  if (absl::Status status = lifecycle_heartbeat->Start(parameter_fetcher);
      status != absl::OkStatus()) {
    return status;
  }

  if (absl::Status status = SetDefaultUdfCodeObject(); !status.ok()) {
    return absl::InternalError(
        "Error setting default UDF. Please contact Google to fix the default "
        "UDF or retry starting the server.");
  }

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
  local_lookup_ = CreateLocalLookup(*cache_);
  auto key_sharder = GetKeySharder(parameter_fetcher);
  auto server_initializer = GetServerInitializer(
      num_shards_, *key_fetcher_manager_, *local_lookup_, environment_,
      shard_num_, *instance_client_, *cache_, parameter_fetcher, key_sharder);
  remote_lookup_ = server_initializer->CreateAndStartRemoteLookupServer();
  {
    auto status_or_notifier =
        BlobStorageChangeNotifier::Create(std::move(metadata));
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
      realtime_notifier_metadata, realtime_thread_numbers);
  if (!maybe_realtime_thread_pool_manager.ok()) {
    return maybe_realtime_thread_pool_manager.status();
  }
  realtime_thread_pool_manager_ =
      std::move(*maybe_realtime_thread_pool_manager);
  data_orchestrator_ = CreateDataOrchestrator(parameter_fetcher, key_sharder);
  TraceRetryUntilOk([this] { return data_orchestrator_->Start(); },
                    "StartDataOrchestrator",
                    LogStatusSafeMetricsFn<kStartDataOrchestratorStatus>());
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

  grpc_server_->GetHealthCheckService()->SetServingStatus(
      std::string(kLoadbalancerHealthcheck), true);
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
      std::move(client_options));
}

std::unique_ptr<StreamRecordReaderFactory>
Server::CreateStreamRecordReaderFactory(
    const ParameterFetcher& parameter_fetcher) {
  const int32_t data_loading_num_threads = parameter_fetcher.GetInt32Parameter(
      kDataLoadingNumThreadsParameterSuffix);
  const std::string file_format = parameter_fetcher.GetParameter(
      kDataLoadingFileFormatSuffix,
      std::string(kFileFormats[static_cast<int>(FileFormat::kRiegeli)]));

  if (file_format == kFileFormats[static_cast<int>(FileFormat::kAvro)]) {
    AvroConcurrentStreamRecordReader::Options options;
    options.num_worker_threads = data_loading_num_threads;
    return std::make_unique<AvroStreamRecordReaderFactory>(options);
  } else if (file_format ==
             kFileFormats[static_cast<int>(FileFormat::kRiegeli)]) {
    ConcurrentStreamRecordReader<std::string_view>::Options options;
    options.num_worker_threads = data_loading_num_threads;
    return std::make_unique<RiegeliStreamRecordReaderFactory>(options);
  }
}

std::unique_ptr<DataOrchestrator> Server::CreateDataOrchestrator(
    const ParameterFetcher& parameter_fetcher, KeySharder key_sharder) {
  const std::string data_bucket =
      parameter_fetcher.GetParameter(kDataBucketParameterSuffix);
  LOG(INFO) << "Retrieved " << kDataBucketParameterSuffix
            << " parameter: " << data_bucket;
  auto metrics_callback =
      LogStatusSafeMetricsFn<kCreateDataOrchestratorStatus>();
  return TraceRetryUntilOk(
      [&] {
        return DataOrchestrator::TryCreate({
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
            .key_sharder = std::move(key_sharder),
            .blob_prefix_allowlist = GetBlobPrefixAllowlist(parameter_fetcher),
        });
      },
      "CreateDataOrchestrator", metrics_callback);
}

void Server::CreateGrpcServices(const ParameterFetcher& parameter_fetcher) {
  const bool use_v2 = parameter_fetcher.GetBoolParameter(kRouteV1ToV2Suffix);
  const bool add_missing_keys_v1 =
      parameter_fetcher.GetBoolParameter(kAddMissingKeysV1Suffix);
  LOG(INFO) << "Retrieved " << kRouteV1ToV2Suffix << " parameter: " << use_v2;
  get_values_adapter_ =
      GetValuesAdapter::Create(std::make_unique<GetValuesV2Handler>(
          *udf_client_, *key_fetcher_manager_));
  GetValuesHandler handler(*cache_, *get_values_adapter_, use_v2,
                           add_missing_keys_v1);
  grpc_services_.push_back(
      std::make_unique<KeyValueServiceImpl>(std::move(handler)));
  GetValuesV2Handler v2handler(*udf_client_, *key_fetcher_manager_);
  grpc_services_.push_back(
      std::make_unique<KeyValueServiceV2Impl>(std::move(v2handler)));
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

  // Increase metadata size, this includes, for example the HTTP URL. Default is
  // 8KB.
  builder.AddChannelArgument(GRPC_ARG_ABSOLUTE_MAX_METADATA_SIZE,
                             32 * 1024);  // Set to 32KB
  builder.AddChannelArgument(GRPC_ARG_MAX_METADATA_SIZE,
                             32 * 1024);  // Set to 32KB
  for (auto& service : grpc_services_) {
    builder.RegisterService(service.get());
  }
  // Finally assemble the server.
  LOG(INFO) << "Server listening on " << server_address << std::endl;
  auto server = builder.BuildAndStart();
  server->GetHealthCheckService()->SetServingStatus(
      std::string(kAutoscalerHealthcheck), true);
  server->GetHealthCheckService()->SetServingStatus(
      std::string(kLoadbalancerHealthcheck), false);
  return std::move(server);
}

absl::Status Server::SetDefaultUdfCodeObject() {
  const absl::Status status = udf_client_->SetCodeObject(
      CodeConfig{.js = kDefaultUdfCodeSnippet,
                 .udf_handler_name = kDefaultUdfHandlerName,
                 .logical_commit_time = kDefaultLogicalCommitTime,
                 .version = kDefaultVersion});
  return status;
}

std::unique_ptr<DeltaFileNotifier> Server::CreateDeltaFileNotifier(
    const ParameterFetcher& parameter_fetcher) {
  uint32_t backup_poll_frequency_secs = parameter_fetcher.GetInt32Parameter(
      kBackupPollFrequencySecsParameterSuffix);
  LOG(INFO) << "Retrieved " << kBackupPollFrequencySecsParameterSuffix
            << " parameter: " << backup_poll_frequency_secs;

  return DeltaFileNotifier::Create(*blob_client_,
                                   absl::Seconds(backup_poll_frequency_secs),
                                   GetBlobPrefixAllowlist(parameter_fetcher));
}

}  // namespace kv_server
