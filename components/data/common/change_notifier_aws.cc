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

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/notification.h"
#include "aws/core/Aws.h"
#include "aws/sns/SNSClient.h"
#include "aws/sns/model/SubscribeRequest.h"
#include "aws/sns/model/SubscribeResult.h"
#include "aws/sqs/SQSClient.h"
#include "aws/sqs/model/CreateQueueRequest.h"
#include "aws/sqs/model/CreateQueueResult.h"
#include "aws/sqs/model/DeleteMessageBatchRequest.h"
#include "aws/sqs/model/DeleteMessageBatchRequestEntry.h"
#include "aws/sqs/model/DeleteMessageRequest.h"
#include "aws/sqs/model/GetQueueAttributesRequest.h"
#include "aws/sqs/model/GetQueueAttributesResult.h"
#include "aws/sqs/model/ReceiveMessageRequest.h"
#include "aws/sqs/model/ReceiveMessageResult.h"
#include "aws/sqs/model/SetQueueAttributesRequest.h"
#include "aws/sqs/model/TagQueueRequest.h"
#include "components/data/common/change_notifier.h"
#include "components/data/common/msg_svc.h"
#include "components/errors/error_util_aws.h"
#include "components/telemetry/server_definition.h"
#include "src/telemetry/telemetry_provider.h"

namespace kv_server {
namespace {

constexpr absl::Duration kMaxLongPollDuration = absl::Seconds(20);
constexpr absl::Duration kLastUpdatedFrequency = absl::Minutes(2);
constexpr char kLastUpdatedTag[] = "last_updated";

class AwsChangeNotifier : public ChangeNotifier {
 public:
  explicit AwsChangeNotifier(
      AwsNotifierMetadata notifier_metadata,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : sns_arn_(std::move(notifier_metadata.sns_arn)),
        queue_manager_(notifier_metadata.queue_manager),
        log_context_(log_context) {
    if (notifier_metadata.only_for_testing_sqs_client_ != nullptr) {
      sqs_.reset(notifier_metadata.only_for_testing_sqs_client_);
    } else {
      // request timeout must be > poll timeout
      // https://github.com/awsdocs/aws-doc-sdk-examples/blob/main/cpp/example_code/sqs/long_polling_on_message_receipt.cpp
      Aws::Client::ClientConfiguration client_cfg;
      client_cfg.requestTimeoutMs =
          absl::ToInt64Milliseconds(kMaxLongPollDuration + absl::Seconds(10));
      sqs_ = std::make_unique<Aws::SQS::SQSClient>(client_cfg);
    }
  }

  absl::StatusOr<std::vector<std::string>> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    PS_LOG(INFO, log_context_)
        << "Getting notifications for topic " << sns_arn_;
    do {
      if (!queue_manager_->IsSetupComplete()) {
        absl::Status status = queue_manager_->SetupQueue();
        if (!status.ok()) {
          PS_LOG(ERROR, log_context_)
              << "Could not set up queue for topic " << sns_arn_;
          LogServerErrorMetric(kAwsChangeNotifierQueueSetupFailure);
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
  std::string GetSqsUrl() {
    auto queue_metadata = queue_manager_->GetQueueMetadata();
    auto aws_queue_metadata = std::get<AwsQueueMetadata>(queue_metadata);
    return aws_queue_metadata.sqs_url;
  }

  absl::Status TagQueue(const std::string& last_updated,
                        const std::string& value) {
    Aws::SQS::Model::TagQueueRequest request;
    request.SetQueueUrl(GetSqsUrl());
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
        PS_LOG(ERROR, log_context_)
            << "Failed to TagQueue with " << kLastUpdatedTag << ": " << tag
            << " " << status;
        LogServerErrorMetric(kAwsChangeNotifierTagFailure);
      }
    }
  }

  absl::StatusOr<std::vector<std::string>> GetNotificationsInternal(
      absl::Duration max_wait) {
    MaybeTagQueue();
    auto receive_message_request_started = absl::Now();
    // Configure request.
    Aws::SQS::Model::ReceiveMessageRequest request;
    request.SetQueueUrl((GetSqsUrl()));
    // Round up to the nearest second.
    request.SetWaitTimeSeconds(
        absl::ToInt64Seconds(absl::Ceil(max_wait, absl::Seconds(1))));
    // Max valid value
    // https://sdk.amazonaws.com/cpp/api/0.12.9/df/d17/class_aws_1_1_s_q_s_1_1_model_1_1_receive_message_request.html#a13311215b25937625b95c86644d5c466
    request.SetMaxNumberOfMessages(10);
    const auto outcome = sqs_->ReceiveMessage(request);
    if (!outcome.IsSuccess()) {
      PS_LOG(ERROR, log_context_) << "Failed to receive message from SQS: "
                                  << outcome.GetError().GetMessage();
      LogServerErrorMetric(kAwsChangeNotifierMessagesReceivingFailure);
      if (!outcome.GetError().ShouldRetry()) {
        // Handle case where recreating Queue will resolve the issue.
        // Example: Queue accidentally deleted.
        PS_LOG(INFO, log_context_) << "Will create a new Queue";
        queue_manager_->Reset();
      }
      return absl::UnavailableError(outcome.GetError().GetMessage());
    }
    const auto& messages = outcome.GetResult().GetMessages();
    if (messages.empty()) {
      return absl::DeadlineExceededError("No messages found.");
    }
    LogIfError(KVServerContextMap()
                   ->SafeMetric()
                   .LogHistogram<kAwsSqsReceiveMessageLatency>(
                       absl::ToDoubleMicroseconds(
                           absl::Now() - receive_message_request_started)));

    std::vector<std::string> keys;
    keys.reserve(messages.size());
    size_t total_message_size;
    for (const auto& message : messages) {
      keys.push_back(message.GetBody());
      total_message_size += message.GetBody().size();
    }
    LogIfError(
        KVServerContextMap()
            ->SafeMetric()
            .template LogHistogram<kReceivedLowLatencyNotificationsBytes>(
                static_cast<int>(total_message_size)));
    LogIfError(KVServerContextMap()
                   ->SafeMetric()
                   .LogUpDownCounter<kReceivedLowLatencyNotificationsCount>(
                       static_cast<int>(messages.size())));

    DeleteMessages(GetSqsUrl(), messages);
    if (keys.empty()) {
      LogServerErrorMetric(kAwsChangeNotifierMessagesDataLoss);
      return absl::DataLossError("All messages invalid.");
    }
    return keys;
  }

  void DeleteMessages(const std::string& queue_url,
                      const Aws::Vector<Aws::SQS::Model::Message>& messages) {
    Aws::Vector<Aws::SQS::Model::DeleteMessageBatchRequestEntry>
        delete_message_batch_request_entries;
    for (const auto& message : messages) {
      Aws::SQS::Model::DeleteMessageBatchRequestEntry
          delete_message_batch_request_entry;
      delete_message_batch_request_entry.SetId(message.GetMessageId());
      delete_message_batch_request_entry.SetReceiptHandle(
          message.GetReceiptHandle());
      delete_message_batch_request_entries.push_back(
          delete_message_batch_request_entry);
    }
    Aws::SQS::Model::DeleteMessageBatchRequest req;
    req.SetQueueUrl(queue_url);
    req.SetEntries(std::move(delete_message_batch_request_entries));
    const auto outcome = sqs_->DeleteMessageBatch(req);
    if (!outcome.IsSuccess()) {
      PS_LOG(ERROR, log_context_) << "Failed to delete message from SQS: "
                                  << outcome.GetError().GetMessage();
      LogServerErrorMetric(kAwsChangeNotifierMessagesDeletionFailure);
    }
  }

  MessageService* queue_manager_;
  const std::string sns_arn_;
  absl::Time last_updated_ = absl::InfinitePast();
  std::unique_ptr<Aws::SQS::SQSClient> sqs_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};
}  // namespace

absl::StatusOr<std::unique_ptr<ChangeNotifier>> ChangeNotifier::Create(
    NotifierMetadata notifier_metadata,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<AwsChangeNotifier>(
      std::move(std::get<AwsNotifierMetadata>(notifier_metadata)), log_context);
}

}  // namespace kv_server
