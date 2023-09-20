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

#include "components/data_server/server/parameter_fetcher.h"
#include "glog/logging.h"

namespace kv_server {

constexpr std::string_view kLocalDirectoryToWatch = "directory";
constexpr std::string_view kRealtimeDirectoryToWatch = "realtime-directory";

NotifierMetadata ParameterFetcher::GetBlobStorageNotifierMetadata() const {
  std::string directory = GetParameter(kLocalDirectoryToWatch);
  LOG(INFO) << "Retrieved " << kLocalDirectoryToWatch
            << " parameter: " << directory;
  return LocalNotifierMetadata{.local_directory = std::move(directory)};
}

NotifierMetadata ParameterFetcher::GetRealtimeNotifierMetadata(
    int32_t num_shards, int32_t shard_num) const {
  std::string directory = GetParameter(kRealtimeDirectoryToWatch);
  LOG(INFO) << "Retrieved " << kRealtimeDirectoryToWatch
            << " parameter: " << directory;
  return LocalNotifierMetadata{.local_directory = std::move(directory)};
}

}  // namespace kv_server
