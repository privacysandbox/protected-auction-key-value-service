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
#include "components/data/realtime/delta_file_record_change_notifier.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/key_value_cache.h"
#include "components/data_server/data_loading/data_orchestrator.h"
#include "components/data_server/server/parameter_fetcher.h"
#include "components/telemetry/metrics_recorder.h"
#include "components/telemetry/telemetry.h"
#include "grpcpp/grpcpp.h"
#include "public/base_types.pb.h"
#include "public/query/get_values.grpc.pb.h"

namespace kv_server {

class Server {
 public:
  Server();

  absl::Status Init(const ParameterClient& parameter_client,
                    InstanceClient& instance_client, std::string environment);

  // Wait for the server to shut down. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  void Wait();

  // Stop the server either gracefully, that may fail after a timeout, or
  // immediately.
  void GracefulShutdown(absl::Duration timeout);
  void ForceShutdown();

 private:
  std::unique_ptr<BlobStorageClient> CreateBlobClient(
      const ParameterFetcher& parameter_fetcher);
  std::unique_ptr<StreamRecordReaderFactory<std::string_view>>
  CreateStreamRecordReaderFactory(const ParameterFetcher& parameter_fetcher);
  std::unique_ptr<DataOrchestrator> CreateDataOrchestrator(
      const ParameterFetcher& parameter_fetcher);

  void CreateGrpcServices(const ParameterFetcher& parameter_fetcher);
  absl::Status MaybeShutdownNotifiers();

  std::unique_ptr<grpc::Server> CreateAndStartGrpcServer();

  std::unique_ptr<DeltaFileNotifier> CreateDeltaFileNotifier(
      const ParameterFetcher& parameter_fetcher);

  MetricsRecorder& metrics_recorder_;
  std::vector<std::unique_ptr<grpc::Service>> grpc_services_;
  std::unique_ptr<grpc::Server> grpc_server_;
  std::unique_ptr<Cache> cache_;

  // BlobStorageClient must outlive DeltaFileNotifier
  std::unique_ptr<BlobStorageClient> blob_client_;

  std::unique_ptr<MessageService> message_service_blob_;
  std::unique_ptr<MessageService> message_service_realtime_;

  // The following fields must outlive DataOrchestrator
  std::unique_ptr<DeltaFileNotifier> notifier_;
  std::unique_ptr<BlobStorageChangeNotifier> change_notifier_;
  std::vector<DataOrchestrator::RealtimeOptions> realtime_options_;
  std::unique_ptr<StreamRecordReaderFactory<std::string_view>>
      delta_stream_reader_factory_;

  std::unique_ptr<DataOrchestrator> data_orchestrator_;
};

absl::Status RunServer();

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_SERVER_SERVER_H_
