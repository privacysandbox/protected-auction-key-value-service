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

// SNS ARN listening to delta file uploads in data bucket, i.e. the standard
// path data load
constexpr std::string_view kDataLoadingFileChannelBucketSNSParameterSuffix =
    "data-loading-file-channel-bucket-sns-arn";

// SNS ARN for realtime high priority updates
constexpr std::string_view kDataLoadingRealtimeChannelSNSParameterSuffix =
    "data-loading-realtime-channel-sns-arn";

// Max connections for AWS's blob storage client
constexpr std::string_view kS3ClientMaxConnectionsParameterSuffix =
    "s3client-max-connections";

// Max buffer size for AWS's blob storage client
constexpr std::string_view kS3ClientMaxRangeBytesParameterSuffix =
    "s3client-max-range-bytes";

NotifierMetadata ParameterFetcher::GetBlobStorageNotifierMetadata() const {
  std::string bucket_sns_arn =
      GetParameter(kDataLoadingFileChannelBucketSNSParameterSuffix);
  PS_LOG(INFO, log_context_)
      << "Retrieved " << kDataLoadingFileChannelBucketSNSParameterSuffix
      << " parameter: " << bucket_sns_arn;
  return AwsNotifierMetadata{.queue_prefix = "BlobNotifier_",
                             .sns_arn = std::move(bucket_sns_arn),
                             .environment = environment_};
}

BlobStorageClient::ClientOptions ParameterFetcher::GetBlobStorageClientOptions()
    const {
  BlobStorageClient::ClientOptions client_options;
  client_options.max_connections =
      GetInt32Parameter(kS3ClientMaxConnectionsParameterSuffix);
  PS_LOG(INFO, log_context_)
      << "Retrieved " << kS3ClientMaxConnectionsParameterSuffix
      << " parameter: " << client_options.max_connections;
  client_options.max_range_bytes =
      GetInt32Parameter(kS3ClientMaxRangeBytesParameterSuffix);
  PS_LOG(INFO, log_context_)
      << "Retrieved " << kS3ClientMaxRangeBytesParameterSuffix
      << " parameter: " << client_options.max_range_bytes;
  return client_options;
}

NotifierMetadata ParameterFetcher::GetRealtimeNotifierMetadata(
    int32_t num_shards, int32_t shard_num) const {
  std::string realtime_sns_arn =
      GetParameter(kDataLoadingRealtimeChannelSNSParameterSuffix);
  PS_LOG(INFO, log_context_)
      << "Retrieved " << kDataLoadingRealtimeChannelSNSParameterSuffix
      << " parameter: " << realtime_sns_arn;
  return AwsNotifierMetadata{"QueueNotifier_", std::move(realtime_sns_arn),
                             .num_shards = num_shards, .shard_num = shard_num,
                             .environment = environment_};
}

}  // namespace kv_server
