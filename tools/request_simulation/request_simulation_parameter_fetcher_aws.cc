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

#include <string>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "tools/request_simulation/request_simulation_parameter_fetcher.h"

ABSL_FLAG(std::string, s3_bucket_sns_arn, "",
          "The Amazon Resource Name(ARN) for the SNS topic"
          "configured for the S3 bucket");

ABSL_FLAG(std::string, realtime_sns_arn, "",
          "The Amazon Resource Name(ARN) for the SNS topic"
          "configured for the realtime updates");

namespace kv_server {

NotifierMetadata
RequestSimulationParameterFetcher::GetBlobStorageNotifierMetadata() const {
  std::string bucket_sns_arn = absl::GetFlag(FLAGS_s3_bucket_sns_arn);
  LOG(INFO) << "The sns arn for s3 bucket is " << bucket_sns_arn;
  return AwsNotifierMetadata{"BlobNotifier_", std::move(bucket_sns_arn)};
}

NotifierMetadata
RequestSimulationParameterFetcher::GetRealtimeNotifierMetadata() const {
  std::string realtime_sns_arn = absl::GetFlag(FLAGS_realtime_sns_arn);
  LOG(INFO) << "The sns arn for s3 bucket is " << realtime_sns_arn;
  return AwsNotifierMetadata{"QueueNotifier_", std::move(realtime_sns_arn)};
}

}  // namespace kv_server
