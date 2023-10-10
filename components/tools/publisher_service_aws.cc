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

#include "aws/sns/SNSClient.h"
#include "aws/sns/model/PublishRequest.h"
#include "components/errors/error_util_aws.h"
#include "components/tools/publisher_service.h"

namespace kv_server {
namespace {

class AwsPublisherService : public PublisherService {
 public:
  explicit AwsPublisherService(std::string sns_arn)
      : sns_arn_(std::move(sns_arn)) {}

  absl::Status Publish(const std::string& body) {
    Aws::SNS::Model::PublishRequest req;
    req.SetTopicArn(sns_arn_);
    req.SetMessage(body);
    Aws::SNS::Model::MessageAttributeValue messageAttributeValue;
    messageAttributeValue.SetDataType("String");
    std::string nanos_since_epoch =
        std::to_string(absl::ToUnixNanos(absl::Now()));
    messageAttributeValue.SetStringValue(nanos_since_epoch);
    req.AddMessageAttributes("time_sent", messageAttributeValue);
    auto outcome = sns_client_.Publish(req);
    return outcome.IsSuccess()
               ? absl::OkStatus()
               : kv_server::AwsErrorToStatus(outcome.GetError());
  }

 private:
  Aws::SNS::SNSClient sns_client_;
  std::string sns_arn_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<PublisherService>> PublisherService::Create(
    NotifierMetadata notifier_metadata) {
  auto metadata = std::get<AwsNotifierMetadata>(notifier_metadata);
  return std::make_unique<AwsPublisherService>(std::move(metadata.sns_arn));
}
}  // namespace kv_server
