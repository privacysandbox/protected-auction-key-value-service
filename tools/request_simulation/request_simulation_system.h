/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TOOLS_REQUEST_SIMULATION_REQUEST_SIMULATION_SYSTEM_H_
#define TOOLS_REQUEST_SIMULATION_REQUEST_SIMULATION_SYSTEM_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/blob_storage/delta_file_notifier.h"
#include "components/util/platform_initializer.h"
#include "grpcpp/grpcpp.h"
#include "public/data_loading/readers/riegeli_stream_io.h"
#include "public/query/get_values.grpc.pb.h"
#include "src/cpp/telemetry/metrics_recorder.h"
#include "tools/request_simulation/client_worker.h"
#include "tools/request_simulation/delta_based_request_generator.h"
#include "tools/request_simulation/message_queue.h"
#include "tools/request_simulation/rate_limiter.h"
#include "tools/request_simulation/request/raw_request.pb.h"
#include "tools/request_simulation/request_simulation_parameter_fetcher.h"
#include "tools/request_simulation/synthetic_request_generator.h"

ABSL_DECLARE_FLAG(std::string, server_address);
ABSL_DECLARE_FLAG(std::string, server_method);
ABSL_DECLARE_FLAG(int64_t, rps);
ABSL_DECLARE_FLAG(int, concurrency);
ABSL_DECLARE_FLAG(absl::Duration, request_timeout);
ABSL_DECLARE_FLAG(int64_t, synthetic_requests_fill_qps);
ABSL_DECLARE_FLAG(int, number_of_keys_per_request);
ABSL_DECLARE_FLAG(int, key_size);
ABSL_DECLARE_FLAG(absl::Duration, rate_limiter_permits_acquire_timeout);
ABSL_DECLARE_FLAG(int, rate_limiter_initial_permits);
ABSL_DECLARE_FLAG(int64_t, message_queue_max_capacity);
ABSL_DECLARE_FLAG(kv_server::GrpcAuthenticationMode,
                  server_authentication_mode);

namespace kv_server {
// The request simulation system has the following key components:
// 1. A message queue that staged the requests waiting to be sent.
// 2. A synthetic request generator that generates made-up requests at given
// rate.
// 3. A delta based request generator that reads keys from delta file and
// generates requests from them
// 4. Client workers that send requests to the target server.
// The number of client workers is determined by the given concurrency
// parameter.
//
// Once the system successfully starts, the system will continuously generates
// requests and send requests to the target server.
class RequestSimulationSystem {
 public:
  RequestSimulationSystem(
      privacy_sandbox::server_common::MetricsRecorder& metrics_recorder,
      privacy_sandbox::server_common::SteadyClock& steady_clock,
      std::unique_ptr<SleepFor> sleep_for,
      absl::AnyInvocable<std::shared_ptr<grpc::Channel>(
          const std::string& server_address,
          const GrpcAuthenticationMode& auth_mode)>
          channel_creation_fn,
      std::unique_ptr<RequestSimulationParameterFetcher>
          parameter_fetcher_for_unit_testing = nullptr)
      : metrics_recorder_(metrics_recorder),
        steady_clock_(steady_clock),
        sleep_for_(std::move(sleep_for)),
        channel_creation_fn_(std::move(channel_creation_fn)) {
    if (parameter_fetcher_for_unit_testing != nullptr) {
      parameter_fetcher_ = std::move(parameter_fetcher_for_unit_testing);
    } else {
      parameter_fetcher_ =
          std::make_unique<RequestSimulationParameterFetcher>();
    }
  }
  // Initializes and starts the system
  absl::Status InitAndStart();
  // Initializes system without starting the system
  absl::Status Init();
  // Starts the system to generate requests and send requests to target server
  absl::Status Start();
  // Stops the system
  absl::Status Stop();
  // Checks if the system is running
  bool IsRunning() const;

  // RequestSimulationSystem is neither copyable nor movable.
  RequestSimulationSystem(const RequestSimulationSystem&) = delete;
  RequestSimulationSystem& operator=(const RequestSimulationSystem&) = delete;

 private:
  std::unique_ptr<BlobStorageClient> CreateBlobClient();
  std::unique_ptr<DeltaFileNotifier> CreateDeltaFileNotifier();
  std::unique_ptr<StreamRecordReaderFactory<std::string_view>>
  CreateStreamRecordReaderFactory();
  std::unique_ptr<RateLimiter> CreateRateLimiter(int64_t per_second_rate);
  absl::Status InitializeGrpcClientWorkers();
  absl::AnyInvocable<std::string(std::string_view)> CreateRequestFromKeyFn();
  // This must be first, otherwise the AWS SDK will crash when it's called:
  PlatformInitializer platform_initializer_;
  privacy_sandbox::server_common::MetricsRecorder& metrics_recorder_;
  std::unique_ptr<MetricsCollector> metrics_collector_;
  privacy_sandbox::server_common::SteadyClock& steady_clock_;
  std::unique_ptr<SleepFor> sleep_for_;
  absl::AnyInvocable<std::shared_ptr<grpc::Channel>(
      const std::string& server_address,
      const GrpcAuthenticationMode& auth_mode)>
      channel_creation_fn_;
  std::string server_address_;
  std::string server_method_;
  int concurrent_number_of_requests_;
  int64_t synthetic_requests_fill_qps_;
  SyntheticRequestGenOption synthetic_request_gen_option_;
  std::unique_ptr<BlobStorageClient> blob_storage_client_;
  std::unique_ptr<MessageService> message_service_blob_;
  std::unique_ptr<BlobStorageChangeNotifier> blob_change_notifier_;
  std::unique_ptr<DeltaFileNotifier> delta_file_notifier_;
  std::unique_ptr<StreamRecordReaderFactory<std::string_view>>
      delta_stream_reader_factory_;
  std::unique_ptr<MessageQueue> message_queue_;
  std::unique_ptr<RateLimiter> synthetic_request_generator_rate_limiter_;
  std::unique_ptr<RateLimiter> grpc_request_rate_limiter_;
  std::unique_ptr<SyntheticRequestGenerator> synthetic_request_generator_;
  std::unique_ptr<DeltaBasedRequestGenerator> delta_based_request_generator_;
  std::vector<std::unique_ptr<ClientWorker<RawRequest, google::api::HttpBody>>>
      grpc_client_workers_;
  std::unique_ptr<RequestSimulationParameterFetcher> parameter_fetcher_;
  bool is_running;
  friend class RequestSimulationSystemTestPeer;
};

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_REQUEST_SIMULATION_SYSTEM_H_
