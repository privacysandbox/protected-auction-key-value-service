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

// TODO(b/296901861): Modify the implementation with GCP specific logic (the
// current implementation is copied from local).

#include <string>

#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "components/data_server/server/parameter_fetcher.h"

namespace kv_server {
constexpr std::string_view kEnvironment = "environment";
constexpr std::string_view kProjectId = "project-id";
constexpr std::string_view kRealtimeUpdaterThreadNumberParameterSuffix =
    "realtime-updater-num-threads";
NotifierMetadata ParameterFetcher::GetBlobStorageNotifierMetadata() const {
  // TODO: set to proper values. Waiting on the change notifier implementation.
  return GcpNotifierMetadata{};
}

BlobStorageClient::ClientOptions ParameterFetcher::GetBlobStorageClientOptions()
    const {
  return BlobStorageClient::ClientOptions();
}

NotifierMetadata ParameterFetcher::GetRealtimeNotifierMetadata(
    int32_t num_shards, int32_t shard_num) const {
  std::string environment = GetParameter(kEnvironment);
  PS_LOG(INFO, log_context_)
      << "Retrieved " << kEnvironment << " parameter: " << environment;
  auto realtime_thread_numbers =
      GetInt32Parameter(kRealtimeUpdaterThreadNumberParameterSuffix);
  PS_LOG(INFO, log_context_)
      << "Retrieved " << kRealtimeUpdaterThreadNumberParameterSuffix
      << " parameter: " << realtime_thread_numbers;
  std::string topic_id =
      absl::StrFormat("kv-server-%s-realtime-pubsub", environment);
  std::string project_id = GetParameter(kProjectId);
  PS_LOG(INFO, log_context_)
      << "Retrieved " << kProjectId << " parameter: " << project_id;
  return GcpNotifierMetadata{
      .queue_prefix = "QueueNotifier_",
      .project_id = project_id,
      .topic_id = topic_id,
      .environment = environment,
      .num_threads = realtime_thread_numbers,
      .num_shards = num_shards,
      .shard_num = shard_num,
  };
}

}  // namespace kv_server
