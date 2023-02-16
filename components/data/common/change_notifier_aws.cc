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
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/notification.h"
#include "aws/core/Aws.h"
#include "aws/core/utils/json/JsonSerializer.h"
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
#include "aws/sqs/model/TagQueueRequest.h"
#include "components/data/common/change_notifier.h"
#include "components/data/common/msg_svc_aws.h"
#include "components/errors/error_util_aws.h"
#include "glog/logging.h"

namespace kv_server {
namespace {

constexpr absl::Duration kMaxLongPollDuration = absl::Seconds(20);
constexpr absl::Duration kLastUpdatedFrequency = absl::Minutes(2);
constexpr char kLastUpdatedTag[] = "last_updated";

class AwsChangeNotifier : public ChangeNotifier {
 public:
  explicit AwsChangeNotifier(std::string queue_prefix, std::string sns_arn)
      : queue_manager_(std::move(queue_prefix), sns_arn),
        sns_arn_(std::move(sns_arn)) {}

  absl::StatusOr<std::vector<std::string>> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    LOG(INFO) << "Getting notifications for topic " << sns_arn_;
    do {
      if (!queue_manager_.IsSetupComplete()) {
        absl::Status status = queue_manager_.SetupQueue();
        if (!status.ok()) {
          LOG(ERROR) << "Could not set up queue for topic " << sns_arn_;
          return status;
        }
      }
      if (max_wait <= kMaxLongPollDuration) {
        return GetNotificationsInternal(max_wait);
      }
      auto keys = GetNotificationsInternal(kMaxLongPollDuration);
      if (keys.ok()) {
        return keys;
      }
      if (!absl::IsDeadlineExceeded(keys.status())) {
        return keys;
      }
      max_wait -= kMaxLongPollDuration;
    } while ((max_wait > absl::ZeroDuration() && !should_stop_callback()));
    return absl::DeadlineExceededError("No messages found.");
  }

 private:
  absl::Status TagQueue(const std::string& last_updated,
                        const std::string& value) {
    Aws::SQS::Model::TagQueueRequest request;
    request.SetQueueUrl(queue_manager_.GetSqsUrl());
    request.AddTags(last_updated, value);
    Aws::SQS::SQSClient sqs;

    const auto outcome = sqs.TagQueue(request);
    return outcome.IsSuccess() ? absl::OkStatus()
                               : AwsErrorToStatus(outcome.GetError());
  }

  void MaybeTagQueue() {
    const absl::Time now = absl::Now();
    if (now - last_updated_ >= kLastUpdatedFrequency) {
      const std::string tag = std::to_string(absl::ToUnixSeconds(now));
      const absl::Status status = TagQueue(kLastUpdatedTag, tag);
      if (status.ok()) {
        last_updated_ = now;
      } else {
        LOG(ERROR) << "Failed to TagQueue with " << kLastUpdatedTag << ": "
                   << tag << " " << status;
      }
    }
  }

  absl::StatusOr<std::vector<std::string>> GetNotificationsInternal(
      absl::Duration max_wait) {
    MaybeTagQueue();
    // request timeout must be > poll timeout
    // https://github.com/awsdocs/aws-doc-sdk-examples/blob/main/cpp/example_code/sqs/long_polling_on_message_receipt.cpp
    Aws::Client::ClientConfiguration client_cfg;
    client_cfg.requestTimeoutMs =
        absl::ToInt64Milliseconds(max_wait + absl::Seconds(10));
    Aws::SQS::SQSClient sqs(client_cfg);
    // Configure request.
    Aws::SQS::Model::ReceiveMessageRequest request;
    request.SetQueueUrl(queue_manager_.GetSqsUrl());
    // Round up to the nearest second.
    request.SetWaitTimeSeconds(
        absl::ToInt64Seconds(absl::Ceil(max_wait, absl::Seconds(1))));
    // Max valid value
    // https://sdk.amazonaws.com/cpp/api/0.12.9/df/d17/class_aws_1_1_s_q_s_1_1_model_1_1_receive_message_request.html#a13311215b25937625b95c86644d5c466
    request.SetMaxNumberOfMessages(10);
    const auto outcome = sqs.ReceiveMessage(request);
    if (!outcome.IsSuccess()) {
      LOG(ERROR) << "Failed to receive message from SQS: "
                 << outcome.GetError().GetMessage();
      if (!outcome.GetError().ShouldRetry()) {
        // Handle case where recreating Queue will resolve the issue.
        // Example: Queue accidentally deleted.
        LOG(INFO) << "Will create a new Queue";
        queue_manager_.Reset();
      }
      return absl::UnavailableError(outcome.GetError().GetMessage());
    }
    const auto& messages = outcome.GetResult().GetMessages();
    if (messages.empty()) {
      return absl::DeadlineExceededError("No messages found.");
    }
    std::vector<std::string> keys;
    for (const auto& message : messages) {
      keys.push_back(message.GetBody());
      DeleteMessage(sqs, queue_manager_.GetSqsUrl(), message);
    }
    if (keys.empty()) {
      return absl::DataLossError("All messages invalid.");
    }
    return keys;
  }

  void DeleteMessage(Aws::SQS::SQSClient sqs, const std::string& queue_url,
                     const Aws::SQS::Model::Message& message) {
    Aws::SQS::Model::DeleteMessageRequest req;
    req.SetQueueUrl(queue_url);
    req.SetReceiptHandle(message.GetReceiptHandle());
    const auto outcome = sqs.DeleteMessage(req);
    if (!outcome.IsSuccess()) {
      LOG(ERROR) << "Failed to delete message from SQS: "
                 << outcome.GetError().GetMessage();
    }
  }

  AwsSnsSqsManager queue_manager_;
  const std::string sns_arn_;
  absl::Time last_updated_ = absl::InfinitePast();
};

}  // namespace

std::unique_ptr<ChangeNotifier> ChangeNotifier::Create(std::string queue_prefix,
                                                       std::string sns_arn) {
  return std::make_unique<AwsChangeNotifier>(std::move(queue_prefix),
                                             std::move(sns_arn));
}

}  // namespace kv_server
