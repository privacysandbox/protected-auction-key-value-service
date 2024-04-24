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

#include <string>

#include "absl/log/log.h"
#include "components/data_server/server/parameter_fetcher.h"

namespace kv_server {

constexpr std::string_view kLocalDirectoryToWatch = "directory";
constexpr std::string_view kRealtimeDirectoryToWatch = "realtime-directory";

NotifierMetadata ParameterFetcher::GetBlobStorageNotifierMetadata() const {
  std::string directory = GetParameter(kLocalDirectoryToWatch);
  PS_LOG(INFO, log_context_)
      << "Retrieved " << kLocalDirectoryToWatch << " parameter: " << directory;
  return LocalNotifierMetadata{.local_directory = std::move(directory)};
}

BlobStorageClient::ClientOptions ParameterFetcher::GetBlobStorageClientOptions()
    const {
  return BlobStorageClient::ClientOptions();
}

NotifierMetadata ParameterFetcher::GetRealtimeNotifierMetadata(
    int32_t num_shards, int32_t shard_num) const {
  std::string directory = GetParameter(kRealtimeDirectoryToWatch);
  PS_LOG(INFO, log_context_) << "Retrieved " << kRealtimeDirectoryToWatch
                             << " parameter: " << directory;
  return LocalNotifierMetadata{.local_directory = std::move(directory)};
}

}  // namespace kv_server
