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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "aws/core/Aws.h"
#include "components/cloud_config/instance_client.h"
#include "components/cloud_config/parameter_client.h"
#include "components/data/blob_storage_client.h"
#include "components/data/delta_file_notifier.h"
#include "components/data/riegeli_stream_io.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/data_loading/data_orchestrator.h"
#include "components/data_server/request_handler/get_values_handler.h"
#include "components/data_server/server/key_value_service_impl.h"
#include "components/errors/retry.h"
#include "glog/logging.h"
#include "grpcpp/ext/proto_server_reflection_plugin.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/health_check_service_interface.h"
#include "public/base_types.pb.h"
#include "public/query/get_values.grpc.pb.h"
#include "src/google/protobuf/struct.pb.h"

ABSL_FLAG(uint16_t, port, 50051,
          "Port the server is listening on. Defaults to 50051.");

namespace fledge::kv_server {

using google::protobuf::Struct;
using google::protobuf::Value;
using grpc::CallbackServerContext;
using grpc::Server;
using grpc::ServerBuilder;

using v1::GetValuesRequest;
using v1::GetValuesResponse;
using v1::KeyValueService;

// TODO: Use config cpio client to get this from the environment
constexpr absl::string_view kParameterPrefix = "kv-server";
constexpr absl::string_view kModeParameterSuffix = "mode";
constexpr absl::string_view kDataBucketParameterSuffix = "data-bucket-id";
constexpr absl::string_view kBucketSNSParameterSuffix = "bucket-sns-arn";
constexpr absl::string_view kLaunchHookParameterSuffix = "launch-hook";

constexpr absl::Duration kRetryBackoff = absl::Seconds(1);

absl::StatusOr<std::string> GetParameter(
    const ParameterClient& parameter_client, absl::string_view environment,
    absl::string_view parameter_suffix) {
  return parameter_client.GetParameter(
      absl::StrJoin({kParameterPrefix, environment, parameter_suffix}, "-"));
}

// Internal server status for demo & debugging. Can add build version and
// others in the future.
void AddInternalKeyValues(ShardedCache& sharded_cache) {
  Cache& cache = sharded_cache.GetMutableCacheShard(KeyNamespace::KV_INTERNAL);
  cache.UpdateKeyValue({"hi", ""},
                       "Hello, world! If you are seeing this, it means you can "
                       "query me successfully");
}

absl::Status RunServer() {
  Aws::SDKOptions options;
  Aws::InitAPI(options);
  // TODO(b/234830172): remove this or turn off by default
  options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Debug;
  absl::Cleanup shutdown = [&options] { Aws::ShutdownAPI(options); };

  int port = absl::GetFlag(FLAGS_port);
  std::string server_address = absl::StrCat("0.0.0.0:", port);

  std::unique_ptr<ShardedCache> cache = ShardedCache::Create();
  AddInternalKeyValues(*cache);

  // TODO(b/235502114): Add reset timeout call in separate thread.

  // Retrieve 'environment' tag. Required for parameters.
  std::unique_ptr<InstanceClient> instance_client = InstanceClient::Create();
  std::string environment = RetryUntilOk(
      [&instance_client]() { return instance_client->GetEnvironmentTag(); },
      "GetEnvironment");
  LOG(INFO) << "Retrieved environment: " << environment;

  // Retrieve 'mode' parameter.
  std::unique_ptr<ParameterClient> parameter_client = ParameterClient::Create();
  std::string mode = RetryUntilOk(
      [&parameter_client, &environment]() {
        return GetParameter(*parameter_client, environment,
                            kModeParameterSuffix);
      },
      "GetModeParameter");

  LOG(INFO) << "Retrieved " << kModeParameterSuffix << " parameter: " << mode;

  // Set up gRPC server
  GetValuesHandler handler(*cache, mode == "DSP");
  KeyValueServiceImpl service(handler);

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  LOG(INFO) << "Server listening on " << server_address << std::endl;

  // Fetch all parameters and set up data orchestrator resources
  std::string bucket_sns_arn = RetryUntilOk(
      [&parameter_client, &environment]() {
        return GetParameter(*parameter_client, environment,
                            kBucketSNSParameterSuffix);
      },
      "GetBucketSNSARNParmeter");
  LOG(INFO) << "Retrieved " << kBucketSNSParameterSuffix
            << " parameter: " << bucket_sns_arn;

  std::unique_ptr<BlobStorageChangeNotifier> change_notifier =
      BlobStorageChangeNotifier::Create({.sns_arn = bucket_sns_arn});
  std::unique_ptr<BlobStorageClient> blob_client = BlobStorageClient::Create();
  std::unique_ptr<DeltaFileNotifier> notifier =
      DeltaFileNotifier::Create(*blob_client);
  std::unique_ptr<StreamRecordReaderFactory<std::string_view>>
      delta_stream_reader_factory =
          StreamRecordReaderFactory<std::string_view>::Create();

  std::string data_bucket = RetryUntilOk(
      [&parameter_client, &environment]() {
        return GetParameter(*parameter_client, environment,
                            kDataBucketParameterSuffix);
      },
      "GetDeltaFileBucketParameter");
  LOG(INFO) << "Retrieved " << kDataBucketParameterSuffix
            << " parameter: " << data_bucket;

  std::string launch_hook_name = RetryUntilOk(
      [&parameter_client, &environment]() {
        return GetParameter(*parameter_client, environment,
                            kLaunchHookParameterSuffix);
      },
      "GetLifecycleHookParameterName");
  LOG(INFO) << "Retrieved " << kLaunchHookParameterSuffix
            << " parameter: " << launch_hook_name;

  // Blocks until cache is initialized
  absl::StatusOr<std::unique_ptr<DataOrchestrator>> maybe_data_orchestrator;
  while (true) {
    maybe_data_orchestrator = DataOrchestrator::TryCreate(
        {.data_bucket = data_bucket,
         .cache = *cache,
         .blob_client = *blob_client,
         .delta_notifier = *notifier,
         .change_notifier = *change_notifier,
         .delta_stream_reader_factory = *delta_stream_reader_factory});
    if (maybe_data_orchestrator.ok()) {
      break;
    } else {
      LOG(WARNING) << "Failed to create data orchestrator: "
                   << maybe_data_orchestrator.status();
      absl::SleepFor(kRetryBackoff);
    }
  }
  while (true) {
    if (const absl::Status start_status = (*maybe_data_orchestrator)->Start();
        start_status.ok()) {
      break;
    } else {
      LOG(WARNING) << "Failed to start data orchestrator: " << start_status;
      absl::SleepFor(kRetryBackoff);
    }
  }
  // TODO(b/235502114): Add retry for Status
  absl::Status complete_lifecycle =
      instance_client->CompleteLifecycle(launch_hook_name);
  if (!complete_lifecycle.ok()) {
    LOG(ERROR) << "Could not complete lifecycle hook " << launch_hook_name
               << ": " << complete_lifecycle;
    return complete_lifecycle;
  }
  LOG(INFO) << "Completed lifecycle hook " << launch_hook_name;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
  return absl::OkStatus();
}
}  // namespace fledge::kv_server

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  const absl::Status status = fledge::kv_server::RunServer();
  if (!status.ok()) {
    LOG(FATAL) << "Failed to run server: " << status;
  }
  return 0;
}
