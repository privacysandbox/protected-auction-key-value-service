// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tools/request_simulation/request_simulation_system.h"

#include <algorithm>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "components/tools/concurrent_publishing_engine.h"
#include "components/tools/util/configure_telemetry_tools.h"
#include "grpcpp/grpcpp.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"
#include "public/data_loading/readers/riegeli_stream_record_reader_factory.h"
#include "public/query/get_values.grpc.pb.h"
#include "src/util/status_macro/status_macros.h"
#include "tools/request_simulation/realtime_message_batcher.h"
#include "tools/request_simulation/request/raw_request.pb.h"
#include "tools/request_simulation/request_generation_util.h"
#include "tools/request_simulation/request_simulation_parameter_fetcher.h"

ABSL_FLAG(std::string, server_address, "",
          "The address of the server under test,"
          "in format demo.kv-server.your-domain.example");
ABSL_FLAG(std::string, server_method,
          "/kv_server.v2.KeyValueService/GetValuesHttp",
          "The api name of the server under test");
ABSL_FLAG(int64_t, rps, 1500, "Requests per second sent to the server");
ABSL_FLAG(int, concurrency, 10,
          "Number of concurrent requests sent to the server,"
          "this number will be limited by the maximum concurrent threads"
          "supported by state of the machine");
ABSL_FLAG(absl::Duration, request_timeout, absl::Seconds(300),
          "The timeout duration for getting response for the request");
ABSL_FLAG(int64_t, synthetic_requests_fill_qps, 1000,
          "The per second rate of synthetic requests generated by the "
          "simulation system");
ABSL_FLAG(int, number_of_keys_per_request, 1,
          "The number of keys in one synthetic request");
ABSL_FLAG(int, key_size, 20, "The size of the key in bytes");
ABSL_FLAG(absl::Duration, client_worker_rate_limiter_acquire_timeout,
          absl::Milliseconds(10),
          "The client worker's timeout duration for acquiring permits from "
          "rate limiter");
ABSL_FLAG(absl::Duration,
          synthetic_requests_generator_rate_limiter_acquire_timeout,
          absl::Seconds(1),
          "The client worker's timeout duration for acquiring permits from "
          "rate limiter");
ABSL_FLAG(int, client_worker_rate_limiter_initial_permits, 0,
          "The initial number of permits available for client workers when the "
          "rate limiter is created");
ABSL_FLAG(int, synthetic_requests_generator_rate_limiter_initial_permits, 1500,
          "The initial number of permits available for synthetic requests "
          "generator when the rate limiter is created");
ABSL_FLAG(int64_t, message_queue_max_capacity, 10000000,
          "The maximum number of messages held by the message queue");
ABSL_FLAG(kv_server::GrpcAuthenticationMode, server_auth_mode,
          kv_server::GrpcAuthenticationMode::kSsl,
          "The server authentication mode");
ABSL_FLAG(bool, is_client_channel, true,
          "Whether the grpc client is connecting to non in-process server");
ABSL_FLAG(int32_t, s3client_max_connections, 1,
          "S3Client max connections for reading data files.");
ABSL_FLAG(int32_t, s3client_max_range_bytes, 8388608,
          "S3Client max range bytes for reading data files.");
ABSL_FLAG(
    int32_t, backup_poll_frequency_secs, 300,
    "Interval between attempts to check if there are new data files on S3,"
    "as a backup to listening to new data files");
ABSL_FLAG(int32_t, data_loading_num_threads, 1,
          "Number of parallel threads for reading and loading data files.");
ABSL_FLAG(std::string, delta_file_bucket, "", "The name of delta file bucket");
ABSL_FLAG(int32_t, num_shards, 1, "Number of shards on the kv-server");
ABSL_FLAG(int32_t, realtime_message_size_kb, 10,
          "Realtime message size threshold in kb");
ABSL_FLAG(int32_t, realtime_publisher_insertion_num_threads, 1,
          "Number of threads used to write to pubsub in parallel.");
ABSL_FLAG(int32_t, realtime_publisher_files_insertion_rate, 15,
          "Number of messages sent per insertion thread to pubsub per second");
ABSL_FLAG(std::string, consented_debug_token, "",
          "Consented debug token, if non-empty value is provided,"
          "consented requests will be sent to the test server");
ABSL_FLAG(bool, use_default_generation_id, true,
          "Whether to send consented requests with default generation_id,"
          "which is a constant for all consented requests. This value should "
          "be set to true if sending high volume of consented traffic to the "
          "server.");

namespace kv_server {

constexpr char* kRequestSimulationServiceName = "request-simulation";
constexpr char* kTestingServer = "testing.server";
constexpr int kMetricsExportIntervalInMs = 5000;
constexpr int kMetricsExportTimeoutInMs = 500;
constexpr char* kDefaultGenerationIdForConsentedRequests = "consented";

using opentelemetry::sdk::resource::Resource;
using opentelemetry::sdk::resource::ResourceAttributes;
using privacy_sandbox::server_common::ConfigurePrivateMetrics;
using privacy_sandbox::server_common::InitTelemetry;
using ::privacy_sandbox::server_common::SleepFor;
using ::privacy_sandbox::server_common::SteadyClock;
namespace semantic_conventions =
    opentelemetry::sdk::resource::SemanticConventions;

std::unique_ptr<RateLimiter> RequestSimulationSystem::CreateRateLimiter(
    int64_t per_second_rate, int64_t initial_permits, absl::Duration timeout,
    std::unique_ptr<SleepFor> sleep_for) {
  return std::make_unique<RateLimiter>(initial_permits, per_second_rate,
                                       steady_clock_, std::move(sleep_for),
                                       timeout);
}

absl::Status RequestSimulationSystem::InitAndStart() {
  if (auto status = Init(); !status.ok()) {
    return status;
  }
  return Start();
}

// TODO(b/289240702) When running on non-local environment,
// populate parameters either from a config file on blob storage
// or from parameter store.
absl::Status RequestSimulationSystem::Init(
    std::unique_ptr<SleepFor> sleep_for_request_generator,
    std::unique_ptr<SleepFor> sleep_for_request_generator_rate_limiter,
    std::unique_ptr<SleepFor> sleep_for_client_worker_rate_limiter,
    std::unique_ptr<MetricsCollector> metrics_collector) {
  server_address_ = absl::GetFlag(FLAGS_server_address);
  server_method_ = absl::GetFlag(FLAGS_server_method);
  consented_debug_token_ = absl::GetFlag(FLAGS_consented_debug_token);
  if (absl::GetFlag(FLAGS_use_default_generation_id)) {
    generation_id_override_ = kDefaultGenerationIdForConsentedRequests;
  }
  concurrent_number_of_requests_ = absl::GetFlag(FLAGS_concurrency);
  synthetic_request_gen_option_.number_of_keys_per_request =
      absl::GetFlag(FLAGS_number_of_keys_per_request);
  synthetic_request_gen_option_.key_size_in_bytes =
      absl::GetFlag(FLAGS_key_size);
  synthetic_requests_fill_qps_ =
      absl::GetFlag(FLAGS_synthetic_requests_fill_qps);
  grpc_request_rate_limiter_ = CreateRateLimiter(
      absl::GetFlag(FLAGS_rps),
      absl::GetFlag(FLAGS_client_worker_rate_limiter_initial_permits),
      absl::GetFlag(FLAGS_client_worker_rate_limiter_acquire_timeout),
      sleep_for_client_worker_rate_limiter == nullptr
          ? std::move(std::make_unique<SleepFor>())
          : std::move(sleep_for_client_worker_rate_limiter));
  synthetic_request_generator_rate_limiter_ = CreateRateLimiter(
      synthetic_requests_fill_qps_,
      absl::GetFlag(
          FLAGS_synthetic_requests_generator_rate_limiter_initial_permits),
      absl::GetFlag(
          FLAGS_synthetic_requests_generator_rate_limiter_acquire_timeout),
      sleep_for_request_generator_rate_limiter == nullptr
          ? std::move(std::make_unique<SleepFor>())
          : std::move(sleep_for_request_generator_rate_limiter));
  message_queue_ = absl::make_unique<MessageQueue>(
      absl::GetFlag(FLAGS_message_queue_max_capacity));
  synthetic_request_generator_ = std::make_unique<SyntheticRequestGenerator>(
      *message_queue_, *synthetic_request_generator_rate_limiter_,
      sleep_for_request_generator == nullptr
          ? std::move(std::make_unique<SleepFor>())
          : std::move(sleep_for_request_generator),
      synthetic_requests_fill_qps_, [this]() {
        const auto keys = kv_server::GenerateRandomKeys(
            synthetic_request_gen_option_.number_of_keys_per_request,
            synthetic_request_gen_option_.key_size_in_bytes);
        return kv_server::CreateKVDSPRequestBodyInJson(
            keys, consented_debug_token_, generation_id_override_);
      });

  // Telemetry must be initialized before initializing metrics collector
  metrics_collector_ =
      metrics_collector == nullptr
          ? std::make_unique<MetricsCollector>(std::make_unique<SleepFor>())
          : std::move(metrics_collector);
  privacy_sandbox::server_common::telemetry::TelemetryConfig config_proto;
  config_proto.set_mode(
      privacy_sandbox::server_common::telemetry::TelemetryConfig::PROD);
  kv_server::KVServerContextMap(
      std::make_unique<
          privacy_sandbox::server_common::telemetry::BuildDependentConfig>(
          config_proto));

  if (auto status = InitializeGrpcClientWorkers(); !status.ok()) {
    return status;
  }
  blob_storage_client_ = CreateBlobClient();
  delta_file_notifier_ = CreateDeltaFileNotifier();
  realtime_delta_file_notifier_ = CreateDeltaFileNotifier();
  delta_stream_reader_factory_ = CreateStreamRecordReaderFactory();

  auto blob_notifier_meta_data =
      parameter_fetcher_->GetBlobStorageNotifierMetadata();
  auto message_service_status = MessageService::Create(blob_notifier_meta_data);
  if (!message_service_status.ok()) {
    return message_service_status.status();
  }
  message_service_blob_ = std::move(*message_service_status);
  SetQueueManager(blob_notifier_meta_data, message_service_blob_.get());
  {
    auto status_or_notifier =
        BlobStorageChangeNotifier::Create(blob_notifier_meta_data);
    if (!status_or_notifier.ok()) {
      // The ChangeNotifier is required to read delta files, if it's not
      // available that's a critical error and so return immediately.
      LOG(ERROR) << "Failed to initialize blob storage change notifier "
                 << status_or_notifier.status();
      return status_or_notifier.status();
    }
    blob_change_notifier_ = std::move(*status_or_notifier);
  }
  PS_ASSIGN_OR_RETURN(
      realtime_blob_change_notifier_,
      BlobStorageChangeNotifier::Create(std::move(blob_notifier_meta_data)));
  delta_based_request_generator_ = std::make_unique<DeltaBasedRequestGenerator>(
      DeltaBasedRequestGenerator::Options{
          .data_bucket = absl::GetFlag(FLAGS_delta_file_bucket),
          .message_queue = *message_queue_,
          .blob_client = *blob_storage_client_,
          .delta_notifier = *delta_file_notifier_,
          .change_notifier = *blob_change_notifier_,
          .delta_stream_reader_factory = *delta_stream_reader_factory_},
      CreateRequestFromKeyFn());
  PS_ASSIGN_OR_RETURN(realtime_message_batcher_,
                      RealtimeMessageBatcher::Create(
                          realtime_messages_, realtime_messages_mutex_,
                          absl::GetFlag(FLAGS_num_shards),
                          absl::GetFlag(FLAGS_realtime_message_size_kb)));
  auto notifier_metadata = parameter_fetcher_->GetRealtimeNotifierMetadata();
  const int realtime_publisher_insertion_num_threads =
      absl::GetFlag(FLAGS_realtime_publisher_insertion_num_threads);
  const int realtime_publisher_files_insertion_rate =
      absl::GetFlag(FLAGS_realtime_publisher_files_insertion_rate);
  concurrent_publishing_engine_ = std::make_unique<ConcurrentPublishingEngine>(
      realtime_publisher_insertion_num_threads, std::move(notifier_metadata),
      realtime_publisher_files_insertion_rate, realtime_messages_mutex_,
      realtime_messages_);
  delta_based_realtime_updates_publisher_ =
      std::make_unique<DeltaBasedRealtimeUpdatesPublisher>(
          std::move(realtime_message_batcher_),
          std::move(concurrent_publishing_engine_),
          DeltaBasedRealtimeUpdatesPublisher::Options{
              .data_bucket = absl::GetFlag(FLAGS_delta_file_bucket),
              .blob_client = *blob_storage_client_,
              .delta_notifier = *realtime_delta_file_notifier_,
              .change_notifier = *realtime_blob_change_notifier_,
              .delta_stream_reader_factory = *delta_stream_reader_factory_,
              .realtime_messages = realtime_messages_,
              .realtime_messages_mutex = realtime_messages_mutex_,
          });

  LOG(INFO) << "Request simulation system is initialized,"
               "target server address is "
            << server_address_ << " and server method is " << server_method_;
  return absl::OkStatus();
}
absl::Status RequestSimulationSystem::InitializeGrpcClientWorkers() {
  if (server_address_.empty()) {
    return absl::FailedPreconditionError("Server address cannot be empty");
  }
  int num_of_workers = std::min(concurrent_number_of_requests_,
                                (int)std::thread::hardware_concurrency());
  if (num_of_workers < 1) {
    return absl::FailedPreconditionError(
        "Could not initialize grpc client worker,"
        "no thread is available to use");
  }

  auto channel = channel_creation_fn_(server_address_,
                                      absl::GetFlag(FLAGS_server_auth_mode));
  bool is_client_channel = absl::GetFlag(FLAGS_is_client_channel);
  auto request_timeout = absl::GetFlag(FLAGS_request_timeout);
  for (int i = 0; i < num_of_workers; ++i) {
    auto request_converter = [](const std::string& request_body) {
      RawRequest request;
      request.mutable_raw_body()->set_data(request_body);
      return request;
    };
    auto worker =
        std::make_unique<ClientWorker<RawRequest, google::api::HttpBody>>(
            i, channel, server_method_, request_timeout, request_converter,
            *message_queue_, *grpc_request_rate_limiter_, *metrics_collector_,
            is_client_channel);
    grpc_client_workers_.push_back(std::move(worker));
  }
  return absl::OkStatus();
}

absl::Status RequestSimulationSystem::Start() {
  LOG(INFO) << "Starting delta based request generator";
  if (auto status = delta_based_request_generator_->Start(); !status.ok()) {
    return status;
  }
  LOG(INFO) << "Starting delta based realtime updates publisher";
  PS_RETURN_IF_ERROR(delta_based_realtime_updates_publisher_->Start());
  if (synthetic_requests_fill_qps_ > 0) {
    LOG(INFO) << "Starting synthetic request generator";
    if (auto status = synthetic_request_generator_->Start(); !status.ok()) {
      return status;
    }
  }

  LOG(INFO) << "Starting " << grpc_client_workers_.size() << " client workers";
  for (const auto& worker : grpc_client_workers_) {
    if (auto status = worker->Start(); !status.ok()) {
      return status;
    }
  }
  LOG(INFO) << "Starting metrics collector";
  if (auto status = metrics_collector_->Start(); !status.ok()) {
    return status;
  }
  is_running = true;
  LOG(INFO) << "Request simulation system is started!";
  return absl::OkStatus();
}
absl::Status RequestSimulationSystem::Stop() {
  LOG(INFO) << "Stopping delta based realtime updates publisher";
  PS_RETURN_IF_ERROR(delta_based_realtime_updates_publisher_->Stop());
  LOG(INFO) << "Stopping delta based request generator";
  if (auto status = delta_based_request_generator_->Stop(); !status.ok()) {
    return status;
  }
  LOG(INFO) << "Stopping synthetic request generator";
  if (auto status = synthetic_request_generator_->Stop(); !status.ok()) {
    return status;
  }
  LOG(INFO) << "Stopping metrics collector";
  if (auto status = metrics_collector_->Stop(); !status.ok()) {
    return status;
  }
  LOG(INFO) << "Stopping client workers";
  for (const auto& worker : grpc_client_workers_) {
    if (auto status = worker->Stop(); !status.ok()) {
      return status;
    }
  }
  LOG(INFO) << "Request simulation system is stopped!";
  is_running = false;
  return absl::OkStatus();
}

bool RequestSimulationSystem::IsRunning() const { return is_running; }

std::unique_ptr<BlobStorageClient> RequestSimulationSystem::CreateBlobClient() {
  BlobStorageClient::ClientOptions options;
  options.max_connections = absl::GetFlag(FLAGS_s3client_max_connections);
  options.max_range_bytes = absl::GetFlag(FLAGS_s3client_max_range_bytes);

  std::unique_ptr<BlobStorageClientFactory> blob_storage_client_factory =
      BlobStorageClientFactory::Create();
  return blob_storage_client_factory->CreateBlobStorageClient(options);
}
std::unique_ptr<DeltaFileNotifier>
RequestSimulationSystem::CreateDeltaFileNotifier() {
  return DeltaFileNotifier::Create(
      *blob_storage_client_,
      absl::Seconds(absl::GetFlag(FLAGS_backup_poll_frequency_secs)));
}
std::unique_ptr<StreamRecordReaderFactory>
RequestSimulationSystem::CreateStreamRecordReaderFactory() {
  ConcurrentStreamRecordReader<std::string_view>::Options options;
  options.num_worker_threads = absl::GetFlag(FLAGS_data_loading_num_threads);
  return std::make_unique<RiegeliStreamRecordReaderFactory>(options);
}
absl::AnyInvocable<std::string(std::string_view)>
RequestSimulationSystem::CreateRequestFromKeyFn() {
  return [this](std::string_view key) {
    return kv_server::CreateKVDSPRequestBodyInJson(
        {std::string(key)}, consented_debug_token_, generation_id_override_);
  };
}
void RequestSimulationSystem::InitializeTelemetry() {
  InitTelemetry("Request-simulation", std::string(BuildVersion()));
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
      metrics_options;
  metrics_options.export_interval_millis =
      std::chrono::milliseconds(kMetricsExportIntervalInMs);
  metrics_options.export_timeout_millis =
      std::chrono::milliseconds(kMetricsExportTimeoutInMs);
  auto server_address = absl::GetFlag(FLAGS_server_address);
  const auto attributes = ResourceAttributes{
      {semantic_conventions::kServiceName, kRequestSimulationServiceName},
      {semantic_conventions::kServiceVersion, std::string(BuildVersion())},
      {semantic_conventions::kHostArch, std::string(BuildPlatform())},
      {kTestingServer, server_address}};
  auto resource = Resource::Create(attributes);
  kv_server::ConfigureTelemetryForTools();
  privacy_sandbox::server_common::telemetry::TelemetryConfig config_proto;
  config_proto.set_mode(
      privacy_sandbox::server_common::telemetry::TelemetryConfig::EXPERIMENT);
  auto* context_map = RequestSimulationContextMap(
      std::make_unique<
          privacy_sandbox::server_common::telemetry::BuildDependentConfig>(
          config_proto),
      ConfigurePrivateMetrics(resource, metrics_options));
}

}  // namespace kv_server
