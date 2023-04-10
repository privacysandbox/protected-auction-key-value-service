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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "components/data_server/request_handler/get_values_handler.h"
#include "components/data_server/request_handler/get_values_v2_handler.h"
#include "components/data_server/server/key_value_service_impl.h"
#include "components/data_server/server/key_value_service_v2_impl.h"
#include "components/data_server/server/lifecycle_heartbeat.h"
#include "components/errors/retry.h"
#include "components/internal_lookup/constants.h"
#include "components/internal_lookup/lookup_client.h"
#include "components/internal_lookup/lookup_server_impl.h"
#include "components/telemetry/init.h"
#include "components/telemetry/kv_telemetry.h"
#include "components/telemetry/telemetry.h"
#include "components/telemetry/telemetry_provider.h"
#include "components/udf/code_fetcher.h"
#include "components/udf/get_values_hook_impl.h"
#include "components/udf/udf_client.h"
#include "components/util/build_info.h"
#include "components/util/platform_initializer.h"
#include "glog/logging.h"
#include "grpcpp/ext/proto_server_reflection_plugin.h"
#include "grpcpp/health_check_service_interface.h"
#include "public/constants.h"
#include "public/data_loading/readers/riegeli_stream_io.h"
#include "public/udf/constants.h"
#include "src/google/protobuf/struct.pb.h"

ABSL_FLAG(uint16_t, port, 50051,
          "Port the server is listening on. Defaults to 50051.");

namespace kv_server {

// TODO: Use config cpio client to get this from the environment
constexpr absl::string_view kModeParameterSuffix = "mode";
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

Server::Server()
    : metrics_recorder_(
          TelemetryProvider::GetInstance().CreateMetricsRecorder()),
      cache_(KeyValueCache::Create()) {
  cache_->UpdateKeyValue(
      "hi",
      "Hello, world! If you are seeing this, it means you can "
      "query me successfully",
      /*logical_commit_time = */ 1);
  cache_->UpdateKeyValue(kUdfCodeSnippetKey, kDefaultUdfCodeSnippet, 1);
  cache_->UpdateKeyValue(kUdfHandlerNameKey, kDefaultUdfHandlerName, 1);
}

absl::Status Server::Init(const ParameterClient& parameter_client,
                          InstanceClient& instance_client,
                          CodeFetcher& code_fetcher,
                          std::unique_ptr<UdfClient> udf_client,
                          std::string environment, std::string instance_id) {
  ConfigureTracer(CreateKVAttributes(std::move(instance_id), environment));
  auto span = GetTracer()->StartSpan("InitServer");
  auto scope = opentelemetry::trace::Scope(span);

  udf_client_ = std::move(udf_client);

  std::unique_ptr<LifecycleHeartbeat> lifecycle_heartbeat =
      LifecycleHeartbeat::Create(instance_client, *metrics_recorder_);

  ParameterFetcher parameter_fetcher(std::move(environment), parameter_client,
                                     metrics_recorder_.get());
  if (absl::Status status = lifecycle_heartbeat->Start(parameter_fetcher);
      status != absl::OkStatus()) {
    return status;
  }
  blob_client_ = CreateBlobClient(parameter_fetcher);
  delta_stream_reader_factory_ =
      CreateStreamRecordReaderFactory(parameter_fetcher);
  notifier_ = CreateDeltaFileNotifier(parameter_fetcher);
  CreateGrpcServices(parameter_fetcher);
  auto metadata = parameter_fetcher.GetBlobStorageNotifierMetadata();
  auto message_service_status = MessageService::Create(metadata);
  if (!message_service_status.ok()) {
    return message_service_status.status();
  }
  message_service_blob_ = std::move(*message_service_status);
  SetQueueManager(metadata, message_service_blob_.get());

  grpc_server_ = CreateAndStartGrpcServer();
  internal_lookup_server_ = CreateAndStartInternalLookupServer();

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
  data_orchestrator_ = CreateDataOrchestrator(parameter_fetcher, *udf_client_);
  TraceRetryUntilOk([this] { return data_orchestrator_->Start(); },
                    "StartDataOrchestrator", metrics_recorder_.get());
  SetUdfCodeObject(code_fetcher);

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
    const ParameterFetcher& parameter_fetcher, UdfClient& udf_client) {
  const std::string data_bucket =
      parameter_fetcher.GetParameter(kDataBucketParameterSuffix);
  LOG(INFO) << "Retrieved " << kDataBucketParameterSuffix
            << " parameter: " << data_bucket;
  auto udf_update_callback = [&udf_client]() {
    const absl::Status status = udf_client.SetCodeObject({});
    if (!status.ok()) {
      LOG(ERROR) << "Error setting code object: " << status;
    }
  };

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
                .udf_update_callback = udf_update_callback,
            },
            *metrics_recorder_);
      },
      "CreateDataOrchestrator", metrics_recorder_.get());
}

void Server::CreateGrpcServices(const ParameterFetcher& parameter_fetcher) {
  const std::string mode = parameter_fetcher.GetParameter(kModeParameterSuffix);
  LOG(INFO) << "Retrieved " << kModeParameterSuffix << " parameter: " << mode;
  GetValuesHandler handler(*cache_, *metrics_recorder_, mode == "DSP");
  grpc_services_.push_back(std::make_unique<KeyValueServiceImpl>(
      std::move(handler), *metrics_recorder_));
  GetValuesV2Handler v2handler(*udf_client_, *metrics_recorder_);
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

std::unique_ptr<grpc::Server> Server::CreateAndStartInternalLookupServer() {
  internal_lookup_service_ = std::make_unique<LookupServiceImpl>(*cache_);

  grpc::ServerBuilder internal_lookup_server_builder;
  internal_lookup_server_builder.AddListeningPort(
      kInternalLookupServerAddress, grpc::InsecureServerCredentials());
  internal_lookup_server_builder.RegisterService(
      internal_lookup_service_.get());

  LOG(INFO) << "Internal lookup server listening on "
            << kInternalLookupServerAddress << std::endl;
  return internal_lookup_server_builder.BuildAndStart();
}

void Server::SetUdfCodeObject(CodeFetcher& code_fetcher) {
  LOG(INFO) << "Fetching UDF Code Snippet";
  auto code_config = TraceRetryUntilOk(
      [&code_fetcher] {
        return code_fetcher.FetchCodeConfig(LookupClient::GetSingleton());
      },
      "FetchUntrustedCodeConfig", metrics_recorder_.get());

  LOG(INFO) << "Setting UDF Code Snippet";
  const absl::Status status =
      udf_client_->SetCodeObject(std::move(code_config));
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

absl::Status RunServer() {
  kv_server::PlatformInitializer initializer;

  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client_or_status =
      UdfClient::Create(UdfClient::ConfigWithGetValuesHook(
          *NewGetValuesHook(&LookupClient::GetSingleton)));
  if (!udf_client_or_status.ok()) {
    return udf_client_or_status.status();
  }

  auto instance_client = InstanceClient::Create();
  std::string environment = TraceRetryUntilOk(
      [&instance_client]() { return instance_client->GetEnvironmentTag(); },
      "GetEnvironment", nullptr);
  LOG(INFO) << "Retrieved environment: " << environment;
  auto parameter_client = ParameterClient::Create();
  auto metrics_options = GetMetricsOptions(*parameter_client, environment);
  auto code_fetcher = CodeFetcher::Create();

  // Retrying getting instance id because it is cached and required
  // for other retryable steps below.  We want it early for metrics.
  std::string instance_id = RetryUntilOk(
      [&instance_client]() { return instance_client->GetInstanceId(); },
      "GetInstanceId", nullptr);
  // Metrics configuration must be called prior to instantiating Server
  InitTelemetry(std::string(kServiceName), std::string(BuildVersion()));
  ConfigureMetrics(CreateKVAttributes(instance_id, environment),
                   metrics_options);
  Server server;
  if (const absl::Status status =
          server.Init(*parameter_client, *instance_client, *code_fetcher,
                      std::move(udf_client_or_status.value()),
                      std::move(environment), std::move(instance_id));
      status != absl::OkStatus()) {
    return status;
  }
  server.Wait();
  server.GracefulShutdown(absl::Seconds(1));
  server.ForceShutdown();
  return absl::OkStatus();
}
}  // namespace kv_server
