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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "aws/sns/SNSClient.h"
#include "aws/sns/model/PublishRequest.h"
#include "components/data/common/msg_svc.h"
#include "components/errors/error_util_aws.h"
#include "components/tools/publisher_service.h"

ABSL_FLAG(std::string, aws_sns_arn, "", "AWS sns arn");

namespace kv_server {
namespace {
const char kQueuePrefix[] = "QueueNotifier_";

class AwsPublisherService : public PublisherService {
 public:
  explicit AwsPublisherService(std::string sns_arn)
      : sns_arn_(std::move(sns_arn)) {}

  absl::Status Publish(const std::string& body, std::optional<int> shard_num) {
    Aws::SNS::Model::PublishRequest req;
    req.SetTopicArn(sns_arn_);
    req.SetMessage(body);
    Aws::SNS::Model::MessageAttributeValue messageAttributeValue;
    messageAttributeValue.SetDataType("String");
    std::string nanos_since_epoch =
        std::to_string(absl::ToUnixNanos(absl::Now()));
    messageAttributeValue.SetStringValue(nanos_since_epoch);
    req.AddMessageAttributes("time_sent", messageAttributeValue);
    if (shard_num.has_value()) {
      Aws::SNS::Model::MessageAttributeValue shardMessageAttributeValue;
      shardMessageAttributeValue.SetDataType("String");
      shardMessageAttributeValue.SetStringValue(
          std::to_string(shard_num.value()));
      req.AddMessageAttributes("shard_num", shardMessageAttributeValue);
    }
    auto outcome = sns_client_.Publish(req);
    return outcome.IsSuccess()
               ? absl::OkStatus()
               : kv_server::AwsErrorToStatus(outcome.GetError());
  }

  absl::StatusOr<NotifierMetadata> BuildNotifierMetadataAndSetQueue() {
    auto maybe_notifier_metadata = PublisherService::GetNotifierMetadata();
    if (!maybe_notifier_metadata.ok()) {
      return maybe_notifier_metadata;
    }
    AwsNotifierMetadata metadata =
        std::get<AwsNotifierMetadata>(maybe_notifier_metadata.value());
    metadata.queue_prefix = kQueuePrefix;
    auto maybe_queue_manager = MessageService::Create(metadata);
    if (!maybe_queue_manager.ok()) {
      return maybe_queue_manager.status();
    }
    queue_manager_ = std::move(*maybe_queue_manager);
    metadata.queue_manager = queue_manager_.get();
    return metadata;
  }

 private:
  Aws::SNS::SNSClient sns_client_;
  std::string sns_arn_;
  std::unique_ptr<kv_server::MessageService> queue_manager_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<PublisherService>> PublisherService::Create(
    NotifierMetadata notifier_metadata) {
  auto metadata = std::get<AwsNotifierMetadata>(notifier_metadata);
  return std::make_unique<AwsPublisherService>(std::move(metadata.sns_arn));
}

absl::StatusOr<NotifierMetadata> PublisherService::GetNotifierMetadata() {
  const std::string sns_arn = absl::GetFlag(FLAGS_aws_sns_arn);
  if (sns_arn.empty()) {
    return absl::InvalidArgumentError(
        "Please specify a full set of parameters for the AWS platform.");
  }
  return AwsNotifierMetadata{.sns_arn = sns_arn};
}

}  // namespace kv_server
