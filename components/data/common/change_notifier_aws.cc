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
#include "components/telemetry/metrics_recorder.h"
#include "components/telemetry/telemetry_provider.h"
#include "glog/logging.h"

namespace kv_server {
namespace {

constexpr absl::Duration kMaxLongPollDuration = absl::Seconds(20);
constexpr absl::Duration kLastUpdatedFrequency = absl::Minutes(2);
constexpr char kLastUpdatedTag[] = "last_updated";
constexpr char* kAwsSqsReceiveMessageLatency = "AwsSqsReceiveMessageLatency";
// Failure events
constexpr char* kAwsChangeNotifierQueueSetupFailure =
    "AwsChangeNotifierQueueSetupFailure";
constexpr char* kAwsChangeNotifierMessagesReceivingFailure =
    "AwsChangeNotifierMessagesReceivingFailure";
constexpr char* kAwsChangeNotifierTagFailure = "AwsChangeNotifierTagFailure";
constexpr char* kAwsChangeNotifierMessagesDeletionFailure =
    "AwsChangeNotifierMessagesDeletionFailure";
constexpr char* kAwsChangeNotifierMessagesDataLossFailure =
    "AwsChangeNotifierMessagesDataLossFailure";

// The units below are microseconds.
const std::vector<double> kAwsSqsReceiveMessageLatencyBucketBoundaries = {
    160,     220,       280,       320,       640,       1'200,         2'500,
    5'000,   10'000,    20'000,    40'000,    80'000,    160'000,       320'000,
    640'000, 1'000'000, 1'300'000, 2'600'000, 5'000'000, 10'000'000'000};

class AwsChangeNotifier : public ChangeNotifier {
 public:
  explicit AwsChangeNotifier(CloudNotifierMetadata notifier_metadata,
                             MetricsRecorder& metrics_recorder)
      : sns_arn_(std::move(notifier_metadata.sns_arn)),
        queue_manager_(notifier_metadata.queue_manager),
        metrics_recorder_(metrics_recorder) {
    // request timeout must be > poll timeout
    // https://github.com/awsdocs/aws-doc-sdk-examples/blob/main/cpp/example_code/sqs/long_polling_on_message_receipt.cpp
    Aws::Client::ClientConfiguration client_cfg;
    client_cfg.requestTimeoutMs =
        absl::ToInt64Milliseconds(kMaxLongPollDuration + absl::Seconds(10));
    sqs_ = Aws::SQS::SQSClient(client_cfg);
    metrics_recorder_.RegisterHistogram(
        kAwsSqsReceiveMessageLatency, "AWS SQS ReceiveMessage latency",
        "microsecond", kAwsSqsReceiveMessageLatencyBucketBoundaries);
  }

  absl::StatusOr<std::vector<std::string>> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    LOG(INFO) << "Getting notifications for topic " << sns_arn_;
    do {
      if (!queue_manager_->IsSetupComplete()) {
        absl::Status status = queue_manager_->SetupQueue();
        if (!status.ok()) {
          LOG(ERROR) << "Could not set up queue for topic " << sns_arn_;
          metrics_recorder_.IncrementEventCounter(
              kAwsChangeNotifierQueueSetupFailure);
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
    request.SetQueueUrl(queue_manager_->GetSqsUrl());
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
        metrics_recorder_.IncrementEventCounter(kAwsChangeNotifierTagFailure);
      }
    }
  }

  absl::StatusOr<std::vector<std::string>> GetNotificationsInternal(
      absl::Duration max_wait) {
    MaybeTagQueue();
    auto receive_message_request_started = absl::Now();
    // Configure request.
    Aws::SQS::Model::ReceiveMessageRequest request;
    request.SetQueueUrl(queue_manager_->GetSqsUrl());
    // Round up to the nearest second.
    request.SetWaitTimeSeconds(
        absl::ToInt64Seconds(absl::Ceil(max_wait, absl::Seconds(1))));
    // Max valid value
    // https://sdk.amazonaws.com/cpp/api/0.12.9/df/d17/class_aws_1_1_s_q_s_1_1_model_1_1_receive_message_request.html#a13311215b25937625b95c86644d5c466
    request.SetMaxNumberOfMessages(10);
    const auto outcome = sqs_.ReceiveMessage(request);
    if (!outcome.IsSuccess()) {
      LOG(ERROR) << "Failed to receive message from SQS: "
                 << outcome.GetError().GetMessage();
      metrics_recorder_.IncrementEventCounter(
          kAwsChangeNotifierMessagesReceivingFailure);
      if (!outcome.GetError().ShouldRetry()) {
        // Handle case where recreating Queue will resolve the issue.
        // Example: Queue accidentally deleted.
        LOG(INFO) << "Will create a new Queue";
        queue_manager_->Reset();
      }
      return absl::UnavailableError(outcome.GetError().GetMessage());
    }
    const auto& messages = outcome.GetResult().GetMessages();
    if (messages.empty()) {
      return absl::DeadlineExceededError("No messages found.");
    }
    metrics_recorder_.RecordHistogramEvent(
        kAwsSqsReceiveMessageLatency,
        absl::ToInt64Microseconds(absl::Now() -
                                  receive_message_request_started));

    std::vector<std::string> keys;
    for (const auto& message : messages) {
      keys.push_back(message.GetBody());
    }
    DeleteMessages(queue_manager_->GetSqsUrl(), messages);
    if (keys.empty()) {
      metrics_recorder_.IncrementEventCounter(
          kAwsChangeNotifierMessagesDataLossFailure);
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
    const auto outcome = sqs_.DeleteMessageBatch(req);
    if (!outcome.IsSuccess()) {
      LOG(ERROR) << "Failed to delete message from SQS: "
                 << outcome.GetError().GetMessage();
      metrics_recorder_.IncrementEventCounter(
          kAwsChangeNotifierMessagesDeletionFailure);
    }
  }

  MessageService* queue_manager_;
  const std::string sns_arn_;
  absl::Time last_updated_ = absl::InfinitePast();
  Aws::SQS::SQSClient sqs_;
  MetricsRecorder& metrics_recorder_;
};
}  // namespace

absl::StatusOr<std::unique_ptr<ChangeNotifier>> ChangeNotifier::Create(
    NotifierMetadata notifier_metadata, MetricsRecorder& metrics_recorder) {
  return std::make_unique<AwsChangeNotifier>(
      std::move(std::get<CloudNotifierMetadata>(notifier_metadata)),
      metrics_recorder);
}

}  // namespace kv_server
