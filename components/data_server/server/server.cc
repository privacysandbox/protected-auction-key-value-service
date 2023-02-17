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

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/str_cat.h"
#include "aws/core/Aws.h"
#include "components/cloud_config/instance_client.h"
#include "components/cloud_config/parameter_client.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/blob_storage/delta_file_notifier.h"
#include "components/data/realtime/delta_file_record_change_notifier.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/key_value_cache.h"
#include "components/data_server/data_loading/data_orchestrator.h"
#include "components/data_server/request_handler/get_values_handler.h"
#include "components/data_server/request_handler/get_values_v2_handler.h"
#include "components/data_server/server/key_value_service_impl.h"
#include "components/data_server/server/key_value_service_v2_impl.h"
#include "components/data_server/server/lifecycle_heartbeat.h"
#include "components/data_server/server/parameter_fetcher.h"
#include "components/errors/retry.h"
#include "components/telemetry/init.h"
#include "components/telemetry/metrics_recorder.h"
#include "components/telemetry/telemetry.h"
#include "components/util/build_info.h"
#include "glog/logging.h"
#include "grpcpp/ext/proto_server_reflection_plugin.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/health_check_service_interface.h"
#include "public/base_types.pb.h"
#include "public/data_loading/readers/riegeli_stream_io.h"
#include "public/query/get_values.grpc.pb.h"
#include "src/google/protobuf/struct.pb.h"

ABSL_FLAG(uint16_t, port, 50051,
          "Port the server is listening on. Defaults to 50051.");

ABSL_FLAG(bool, buildinfo, false, "Print build info.");

namespace kv_server {

using google::protobuf::Struct;
using google::protobuf::Value;
using grpc::CallbackServerContext;

// TODO: Use config cpio client to get this from the environment
constexpr absl::string_view kModeParameterSuffix = "mode";
constexpr absl::string_view kDataBucketParameterSuffix = "data-bucket-id";
// SNS ARN listening to delta file uploads in data bucket, i.e. the standard
// path data load
constexpr absl::string_view kDataLoadingFileChannelBucketSNSParameterSuffix =
    "data-loading-file-channel-bucket-sns-arn";
// SNS ARN for realtime high priority updates
constexpr absl::string_view kDataLoadingRealtimeChannelSNSParameterSuffix =
    "data-loading-realtime-channel-sns-arn";
constexpr absl::string_view kBackupPollFrequencySecsParameterSuffix =
    "backup-poll-frequency-secs";
constexpr absl::string_view kMetricsExportIntervalMillisParameterSuffix =
    "metrics-export-interval-millis";
constexpr absl::string_view kMetricsExportTimeoutMillisParameterSuffix =
    "metrics-export-timeout-millis";

std::unique_ptr<BlobStorageChangeNotifier> CreateChangeNotifier(
    const ParameterFetcher& parameter_fetcher) {
  std::string bucket_sns_arn = parameter_fetcher.GetParameter(
      kDataLoadingFileChannelBucketSNSParameterSuffix);
  LOG(INFO) << "Retrieved " << kDataLoadingFileChannelBucketSNSParameterSuffix
            << " parameter: " << bucket_sns_arn;
  return BlobStorageChangeNotifier::Create({.sns_arn = bucket_sns_arn});
}

std::unique_ptr<DeltaFileRecordChangeNotifier>
CreateDeltaFileRecordChangeNotifier(const ParameterFetcher& parameter_fetcher) {
  std::string realtime_sns_arn = parameter_fetcher.GetParameter(
      kDataLoadingRealtimeChannelSNSParameterSuffix);
  LOG(INFO) << "Retrieved " << kDataLoadingRealtimeChannelSNSParameterSuffix
            << " parameter: " << realtime_sns_arn;
  return DeltaFileRecordChangeNotifier::Create({.sns_arn = realtime_sns_arn});
}

class Server {
 public:
  Server()
      : metrics_recorder_(MetricsRecorder::Create()),
        cache_(KeyValueCache::Create()),
        blob_client_(BlobStorageClient::Create()),
        delta_stream_reader_factory_(
            StreamRecordReaderFactory<std::string_view>::Create()) {
    cache_->UpdateKeyValue(
        "hi",
        "Hello, world! If you are seeing this, it means you can "
        "query me successfully",
        /*logical_commit_time = */ 1);
  }

  absl::Status Init(const ParameterClient& parameter_client,
                    InstanceClient& instance_client, std::string environment) {
    auto span = GetTracer()->StartSpan("InitServer");
    auto scope = opentelemetry::trace::Scope(span);

    std::unique_ptr<LifecycleHeartbeat> lifecycle_heartbeat =
        LifecycleHeartbeat::Create(instance_client, *metrics_recorder_);

    std::unique_ptr<ParameterFetcher> parameter_fetcher =
        ParameterFetcher::Create(std::move(environment), parameter_client,
                                 *metrics_recorder_);
    if (absl::Status status = lifecycle_heartbeat->Start(*parameter_fetcher);
        status != absl::OkStatus()) {
      return status;
    }
    delta_file_thread_notifier_ = ThreadNotifier::Create("Delta file notifier");
    realtime_thread_notifier_ = ThreadNotifier::Create("Realtime notifier");

    notifier_ = CreateDeltaFileNotifier(*parameter_fetcher);
    CreateGrpcServices(*parameter_fetcher);
    grpc_server_ = CreateAndStartGrpcServer();
    change_notifier_ = CreateChangeNotifier(*parameter_fetcher);
    delta_file_record_change_notifier_ =
        CreateDeltaFileRecordChangeNotifier(*parameter_fetcher);
    realtime_notifier_ = RealtimeNotifier::Create(*realtime_thread_notifier_);
    data_orchestrator_ = CreateDataOrchestrator(*parameter_fetcher);
    TraceRetryUntilOk([this] { return data_orchestrator_->Start(); },
                      "StartDataOrchestrator", *metrics_recorder_);
    return absl::OkStatus();
  }

  // Wait for the server to shut down. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  void Wait() { grpc_server_->Wait(); }

 private:
  std::unique_ptr<DataOrchestrator> CreateDataOrchestrator(
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
                  .delta_file_record_change_notifier =
                      *delta_file_record_change_notifier_,
                  .realtime_notifier = *realtime_notifier_,
              },
              *metrics_recorder_);
        },
        "CreateDataOrchestrator", *metrics_recorder_);
  }

  void CreateGrpcServices(const ParameterFetcher& parameter_fetcher) {
    const std::string mode =
        parameter_fetcher.GetParameter(kModeParameterSuffix);
    LOG(INFO) << "Retrieved " << kModeParameterSuffix << " parameter: " << mode;
    GetValuesHandler handler(*cache_, *metrics_recorder_, mode == "DSP");
    grpc_services_.push_back(std::make_unique<KeyValueServiceImpl>(
        std::move(handler), *metrics_recorder_));
    GetValuesV2Handler v2handler(*cache_, *metrics_recorder_);
    grpc_services_.push_back(std::make_unique<KeyValueServiceV2Impl>(
        std::move(v2handler), *metrics_recorder_));
  }

  std::unique_ptr<grpc::Server> CreateAndStartGrpcServer() {
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

  std::unique_ptr<DeltaFileNotifier> CreateDeltaFileNotifier(
      const ParameterFetcher& parameter_fetcher) {
    uint32_t backup_poll_frequency_secs = parameter_fetcher.GetInt32Parameter(
        kBackupPollFrequencySecsParameterSuffix);
    LOG(INFO) << "Retrieved " << kBackupPollFrequencySecsParameterSuffix
              << " parameter: " << backup_poll_frequency_secs;

    return DeltaFileNotifier::Create(*delta_file_thread_notifier_,
                                     *blob_client_,
                                     absl::Seconds(backup_poll_frequency_secs));
  }

  std::unique_ptr<MetricsRecorder> metrics_recorder_;
  std::vector<std::unique_ptr<grpc::Service>> grpc_services_;
  std::unique_ptr<grpc::Server> grpc_server_;
  std::unique_ptr<Cache> cache_;

  // BlobStorageClient must outlive DeltaFileNotifier
  std::unique_ptr<BlobStorageClient> blob_client_;

  // The following fields must outlive DataOrchestrator
  std::unique_ptr<ThreadNotifier> delta_file_thread_notifier_;
  std::unique_ptr<DeltaFileNotifier> notifier_;
  std::unique_ptr<BlobStorageChangeNotifier> change_notifier_;
  std::unique_ptr<DeltaFileRecordChangeNotifier>
      delta_file_record_change_notifier_;
  std::unique_ptr<ThreadNotifier> realtime_thread_notifier_;
  std::unique_ptr<RealtimeNotifier> realtime_notifier_;
  std::unique_ptr<StreamRecordReaderFactory<std::string_view>>
      delta_stream_reader_factory_;

  std::unique_ptr<DataOrchestrator> data_orchestrator_;
};

opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
GetMetricsOptions(const ParameterClient& parameter_client,
                  MetricsRecorder& blank_metrics_recorder,
                  const std::string environment) {
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
      metrics_options;

  std::unique_ptr<ParameterFetcher> parameter_fetcher =
      ParameterFetcher::Create(environment, parameter_client,
                               blank_metrics_recorder);

  uint32_t export_interval_millis = parameter_fetcher->GetInt32Parameter(
      kMetricsExportIntervalMillisParameterSuffix);
  LOG(INFO) << "Retrieved " << kMetricsExportIntervalMillisParameterSuffix
            << " parameter: " << export_interval_millis;
  uint32_t export_timeout_millis = parameter_fetcher->GetInt32Parameter(
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
  Aws::SDKOptions options;
  Aws::InitAPI(options);
  // TODO(b/234830172): remove this or turn off by default
  options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Debug;
  absl::Cleanup shutdown = [&options] { Aws::ShutdownAPI(options); };

  LogBuildInfo();
  if (absl::GetFlag(FLAGS_buildinfo)) {
    return absl::OkStatus();
  }

  auto instance_client = InstanceClient::Create();
  auto blank_metrics_recorder = MetricsRecorder::CreateBlank();

  std::string environment = TraceRetryUntilOk(
      [&instance_client]() { return instance_client->GetEnvironmentTag(); },
      "GetEnvironment", *blank_metrics_recorder);
  LOG(INFO) << "Retrieved environment: " << environment;

  auto parameter_client = ParameterClient::Create();

  auto metrics_options = GetMetricsOptions(
      *parameter_client, *blank_metrics_recorder, environment);

  // Retrying getting instance id because it is cached and required
  // for other retryable steps below.  We want it early for metrics.
  std::string instance_id = RetryUntilOk(
      [&instance_client]() { return instance_client->GetInstanceId(); },
      "GetInstanceId", *blank_metrics_recorder);
  // InitMetrics must be called prior to instantiating Server
  InitMetrics(instance_id, metrics_options);
  InitTracer(std::move(instance_id));
  Server server;
  if (const absl::Status status = server.Init(
          *parameter_client, *instance_client, std::move(environment));
      status != absl::OkStatus()) {
    return status;
  }
  server.Wait();
  return absl::OkStatus();
}
}  // namespace kv_server

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  const absl::Status status = kv_server::RunServer();
  if (!status.ok()) {
    LOG(FATAL) << "Failed to run server: " << status;
  }
  return 0;
}
