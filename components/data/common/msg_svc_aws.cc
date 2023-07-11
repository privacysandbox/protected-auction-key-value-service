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
#include "aws/sqs/model/ReceiveMessageResult.h"
#include "aws/sqs/model/SetQueueAttributesRequest.h"
#include "components/data/common/msg_svc.h"
#include "components/errors/error_util_aws.h"

namespace kv_server {
namespace {
constexpr uint32_t kQueueNameLen = 80;
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

constexpr std::string_view alphanum =
    "_-0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";

class AwsMessageService : public MessageService {
 public:
  // `prefix` is the prefix of randomly generated SQS Queue name.
  // The queue is subscribed to the topic at `sns_arn`.
  AwsMessageService(std::string prefix, std::string sns_arn,
                    std::optional<int32_t> shard_num)
      : prefix_(std::move(prefix)),
        sns_arn_(std::move(sns_arn)),
        shard_num_(shard_num) {}

  bool IsSetupComplete() const {
    absl::ReaderMutexLock lock(&mutex_);
    return is_set_up_;
  }

  // Returns url if `IsSetupComplete` is true, or empty string otherwise.
  const std::string& GetSqsUrl() const {
    absl::ReaderMutexLock lock(&mutex_);
    return sqs_url_;
  }

  absl::Status SetupQueue() {
    absl::MutexLock lock(&mutex_);
    if (sqs_url_.empty()) {
      absl::StatusOr<std::string> url = CreateQueue(sqs_client_, prefix_);
      if (!url.ok()) {
        return url.status();
      }
      sqs_url_ = std::move(*url);
    }
    // TODO: Any non-retryable status from this point on should result in a
    // reset.
    if (sqs_arn_.empty()) {
      absl::StatusOr<std::string> arn = GetQueueArn(sqs_client_, sqs_url_);
      if (!arn.ok()) {
        return arn.status();
      }
      sqs_arn_ = std::move(*arn);
    }
    if (!are_attributes_set_) {
      auto result =
          SetQueueAttributes(sqs_client_, sns_arn_, sqs_arn_, sqs_url_);

      if (!result.ok()) {
        return result;
      }
      are_attributes_set_ = true;
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
  std::string GenerateQueueName(std::string name) {
    absl::BitGen bitgen;
    while (name.length() < kQueueNameLen) {
      const size_t index = absl::Uniform(bitgen, 0u, alphanum.length() - 1);
      name += alphanum[index];
    }
    return name;
  }

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

  absl::StatusOr<std::string> GetQueueArn(Aws::SQS::SQSClient& sqs,
                                          const std::string& sqs_url) {
    Aws::SQS::Model::GetQueueAttributesRequest req;
    req.SetQueueUrl(sqs_url);
    req.AddAttributeNames(Aws::SQS::Model::QueueAttributeName::QueueArn);
    const auto outcome = sqs.GetQueueAttributes(req);
    if (outcome.IsSuccess()) {
      return outcome.GetResult().GetAttributes().at(
          Aws::SQS::Model::QueueAttributeName::QueueArn);
    }
    return AwsErrorToStatus(outcome.GetError());
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
  bool is_set_up_ = false;
  std::string sqs_url_;
  std::string sqs_arn_;
  bool are_attributes_set_ = false;
  std::optional<int32_t> shard_num_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<MessageService>> MessageService::Create(
    NotifierMetadata notifier_metadata, std::optional<int32_t> shard_num) {
  auto metadata = std::get<CloudNotifierMetadata>(notifier_metadata);

  return std::make_unique<AwsMessageService>(
      std::move(metadata.queue_prefix), std::move(metadata.sns_arn), shard_num);
}
}  // namespace kv_server
