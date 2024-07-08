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

#include <optional>

#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "aws/core/Aws.h"
#include "aws/sns/SNSClient.h"
#include "aws/sns/model/SubscribeRequest.h"
#include "aws/sns/model/SubscribeResult.h"
#include "aws/sqs/SQSClient.h"
#include "aws/sqs/model/CreateQueueRequest.h"
#include "aws/sqs/model/CreateQueueResult.h"
#include "aws/sqs/model/DeleteMessageRequest.h"
#include "aws/sqs/model/GetQueueAttributesRequest.h"
#include "aws/sqs/model/GetQueueAttributesResult.h"
#include "aws/sqs/model/ReceiveMessageRequest.h"
#include "aws/sqs/model/SetQueueAttributesRequest.h"
#include "aws/sqs/model/TagQueueRequest.h"
#include "components/data/common/msg_svc.h"
#include "components/data/common/msg_svc_util.h"
#include "components/errors/error_util_aws.h"
#include "src/util/status_macro/status_macros.h"

namespace kv_server {
namespace {
constexpr char kPolicyTemplate[] = R"({
  "Version": "2008-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Principal": {
        "Service": "sns.amazonaws.com"
      },
      "Resource": "%s",
      "Action": "sqs:SendMessage",
      "Condition": {
        "ArnEquals": {
          "aws:SourceArn": "%s"
        }
      }
    }
  ]
})";

constexpr char kFilterPolicyTemplate[] = R"({
  "shard_num": ["%d"]
})";
constexpr char kEnvironmentTag[] = "environment";

class AwsMessageService : public MessageService {
 public:
  // `prefix` is the prefix of randomly generated SQS Queue name.
  // The queue is subscribed to the topic at `sns_arn`.
  AwsMessageService(
      std::string prefix, std::string sns_arn, std::string environment,
      std::optional<int32_t> shard_num,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : prefix_(std::move(prefix)),
        sns_arn_(std::move(sns_arn)),
        environment_(std::move(environment)),
        shard_num_(shard_num),
        log_context_(log_context) {}

  bool IsSetupComplete() const {
    absl::ReaderMutexLock lock(&mutex_);
    return is_set_up_;
  }

  // Returns url if `IsSetupComplete` is true, or empty string otherwise.
  const QueueMetadata GetQueueMetadata() const {
    absl::ReaderMutexLock lock(&mutex_);
    AwsQueueMetadata metadata = {.sqs_url = sqs_url_};
    return metadata;
  }

  absl::Status SetupQueue() {
    absl::MutexLock lock(&mutex_);
    if (sqs_url_.empty()) {
      PS_ASSIGN_OR_RETURN(sqs_url_, CreateQueue(sqs_client_, prefix_));
    }
    // TODO: Any non-retryable status from this point on should result in a
    // reset.
    if (sqs_arn_.empty()) {
      PS_ASSIGN_OR_RETURN(sqs_arn_, GetQueueArn(sqs_client_, sqs_url_));
    }
    if (!are_attributes_set_) {
      PS_RETURN_IF_ERROR(
          SetQueueAttributes(sqs_client_, sns_arn_, sqs_arn_, sqs_url_));
      are_attributes_set_ = true;
    }
    if (!environment_.empty()) {
      PS_RETURN_IF_ERROR(TagQueue(sqs_client_, sqs_url_));
    }
    const absl::Status status = SubscribeQueue(sns_client_, sns_arn_, sqs_arn_);
    if (status.ok()) {
      is_set_up_ = true;
    }
    return status;
  }

  void Reset() {
    absl::MutexLock lock(&mutex_);
    sqs_url_ = "";
    sqs_arn_ = "";
    are_attributes_set_ = false;
    is_set_up_ = false;
  }

 private:
  absl::StatusOr<std::string> CreateQueue(Aws::SQS::SQSClient& sqs,
                                          const std::string& prefix) {
    Aws::SQS::Model::CreateQueueRequest req;
    const std::string name = GenerateQueueName(prefix);
    req.SetQueueName(name);
    const auto outcome = sqs.CreateQueue(req);
    if (outcome.IsSuccess()) {
      return outcome.GetResult().GetQueueUrl();
    }
    return AwsErrorToStatus(outcome.GetError());
  }

  absl::Status SetQueueAttributes(Aws::SQS::SQSClient& sqs,
                                  const std::string& sns_arn,
                                  const std::string& sqs_arn,
                                  const std::string& sqs_url) {
    Aws::SQS::Model::SetQueueAttributesRequest req;
    req.SetQueueUrl(sqs_url);
    req.AddAttributes(Aws::SQS::Model::QueueAttributeName::Policy,
                      absl::StrFormat(kPolicyTemplate, sqs_arn, sns_arn));
    const auto outcome = sqs.SetQueueAttributes(req);
    return outcome.IsSuccess() ? absl::OkStatus()
                               : AwsErrorToStatus(outcome.GetError());
  }

  absl::Status TagQueue(Aws::SQS::SQSClient& sqs, const std::string& sqs_url) {
    Aws::SQS::Model::TagQueueRequest request;
    request.SetQueueUrl(sqs_url);
    request.AddTags(kEnvironmentTag, environment_);

    const auto outcome = sqs.TagQueue(request);
    return outcome.IsSuccess() ? absl::OkStatus()
                               : AwsErrorToStatus(outcome.GetError());
  }

  absl::StatusOr<std::string> GetQueueArn(Aws::SQS::SQSClient& sqs,
                                          const std::string& sqs_url) {
    Aws::SQS::Model::GetQueueAttributesRequest req;
    req.SetQueueUrl(sqs_url);
    req.AddAttributeNames(Aws::SQS::Model::QueueAttributeName::QueueArn);

    if (const auto outcome = sqs.GetQueueAttributes(req); outcome.IsSuccess()) {
      return outcome.GetResult().GetAttributes().at(
          Aws::SQS::Model::QueueAttributeName::QueueArn);
    } else {
      return AwsErrorToStatus(outcome.GetError());
    }
  }

  absl::Status SubscribeQueue(Aws::SNS::SNSClient& sns,
                              const std::string& sns_arn,
                              const std::string& queue_url) {
    Aws::SNS::Model::SubscribeRequest req;
    req.SetTopicArn(sns_arn);
    req.SetProtocol("sqs");
    req.SetEndpoint(queue_url);
    if (prefix_ == "QueueNotifier_" && shard_num_.has_value()) {
      req.AddAttributes("FilterPolicy", absl::StrFormat(kFilterPolicyTemplate,
                                                        shard_num_.value()));
    }
    const auto outcome = sns.Subscribe(req);
    return outcome.IsSuccess() ? absl::OkStatus()
                               : AwsErrorToStatus(outcome.GetError());
  }

  mutable absl::Mutex mutex_;
  Aws::SQS::SQSClient sqs_client_;
  Aws::SNS::SNSClient sns_client_;
  const std::string prefix_;
  const std::string sns_arn_;
  const std::string environment_;
  bool is_set_up_ = false;
  std::string sqs_url_;
  std::string sqs_arn_;
  bool are_attributes_set_ = false;
  std::optional<int32_t> shard_num_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<MessageService>> MessageService::Create(
    NotifierMetadata notifier_metadata,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  auto metadata = std::get<AwsNotifierMetadata>(notifier_metadata);
  auto shard_num =
      (metadata.num_shards > 1 ? std::optional<int32_t>(metadata.shard_num)
                               : std::nullopt);
  return std::make_unique<AwsMessageService>(
      std::move(metadata.queue_prefix), std::move(metadata.sns_arn),
      std::move(metadata.environment), shard_num, log_context);
}
}  // namespace kv_server
