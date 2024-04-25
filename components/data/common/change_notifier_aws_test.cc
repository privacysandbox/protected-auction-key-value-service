/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <utility>

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "aws/sqs/SQSClient.h"
#include "aws/sqs/model/DeleteMessageBatchRequest.h"
#include "aws/sqs/model/ReceiveMessageRequest.h"
#include "components/data/common/change_notifier.h"
#include "components/data/common/msg_svc.h"
#include "components/telemetry/server_definition.h"
#include "components/util/platform_initializer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

class MockMessageService : public MessageService {
 public:
  MOCK_METHOD(bool, IsSetupComplete, (), (const));

  MOCK_METHOD(const QueueMetadata, GetQueueMetadata, (), (const));

  MOCK_METHOD(absl::Status, SetupQueue, (), ());

  MOCK_METHOD(void, Reset, (), ());
};

class MockSqsClient : public ::Aws::SQS::SQSClient {
 public:
  MOCK_METHOD(Aws::SQS::Model::ReceiveMessageOutcome, ReceiveMessage,
              (const Aws::SQS::Model::ReceiveMessageRequest& request),
              (const, override));

  MOCK_METHOD(Aws::SQS::Model::DeleteMessageBatchOutcome, DeleteMessageBatch,
              (const Aws::SQS::Model::DeleteMessageBatchRequest& request),
              (const, override));
};

class ChangeNotifierAwsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    privacy_sandbox::server_common::telemetry::TelemetryConfig config_proto;
    config_proto.set_mode(
        privacy_sandbox::server_common::telemetry::TelemetryConfig::PROD);
    kv_server::KVServerContextMap(
        privacy_sandbox::server_common::telemetry::BuildDependentConfig(
            config_proto));
  }

 private:
  PlatformInitializer initializer_;
};

TEST_F(ChangeNotifierAwsTest, SmokeTest) {
  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<ChangeNotifier>> notifier =
      ChangeNotifier::Create(notifier_metadata);
  EXPECT_TRUE(notifier.status().ok());
}

TEST_F(ChangeNotifierAwsTest, ReceiveMessageFails) {
  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  MockMessageService mock_message_service;
  EXPECT_CALL(mock_message_service, IsSetupComplete)
      .WillOnce(::testing::Return(true));
  const std::string mock_sqs_url("mock sqs url");
  AwsQueueMetadata metadata = {.sqs_url = mock_sqs_url};
  EXPECT_CALL(mock_message_service, GetQueueMetadata())
      .WillRepeatedly(::testing::Return(metadata));
  // A default ReceiveMessageOutcome will be returned for calls to
  // mock_sqs_client.ReceiveMessage(_).

  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.queue_manager = &mock_message_service;
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<ChangeNotifier>> notifier =
      ChangeNotifier::Create(notifier_metadata);
  ASSERT_TRUE(notifier.status().ok());

  const absl::StatusOr<std::vector<std::string>> notifications =
      (*notifier)->GetNotifications(absl::Seconds(1), [] { return false; });
  EXPECT_EQ(::absl::StatusCode::kUnavailable, notifications.status().code());
}

TEST_F(ChangeNotifierAwsTest,
       ReceiveMessagePassesButNoMessageListedSoDeadlineExceeded) {
  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  MockMessageService mock_message_service;
  EXPECT_CALL(mock_message_service, IsSetupComplete)
      .WillOnce(::testing::Return(true));
  const std::string mock_sqs_url("mock sqs url");
  AwsQueueMetadata metadata = {.sqs_url = mock_sqs_url};
  EXPECT_CALL(mock_message_service, GetQueueMetadata())
      .WillRepeatedly(::testing::Return(metadata));

  Aws::SQS::Model::ReceiveMessageResult result;
  // Because we populate the Outcome with a Result that means that IsSuccess()
  // will return true.
  Aws::SQS::Model::ReceiveMessageOutcome outcome(result);
  EXPECT_CALL(*mock_sqs_client, ReceiveMessage(::testing::_))
      .WillOnce(::testing::Return(outcome));

  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.queue_manager = &mock_message_service;
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<ChangeNotifier>> notifier =
      ChangeNotifier::Create(notifier_metadata);
  ASSERT_TRUE(notifier.status().ok());

  const absl::StatusOr<std::vector<std::string>> notifications =
      (*notifier)->GetNotifications(absl::Seconds(1), [] { return false; });
  EXPECT_EQ(::absl::StatusCode::kDeadlineExceeded,
            notifications.status().code());
}

TEST_F(ChangeNotifierAwsTest, ReceiveMessagePassesAndHasMessages) {
  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  MockMessageService mock_message_service;
  EXPECT_CALL(mock_message_service, IsSetupComplete)
      .WillOnce(::testing::Return(true));
  const std::string mock_sqs_url("mock sqs url");
  AwsQueueMetadata metadata = {.sqs_url = mock_sqs_url};
  EXPECT_CALL(mock_message_service, GetQueueMetadata())
      .WillRepeatedly(::testing::Return(metadata));

  Aws::SQS::Model::ReceiveMessageResult result;
  Aws::SQS::Model::Message message;
  message.SetBody("mock message body");
  result.AddMessages(message);
  // Because we populate the Outcome with a Result that means that IsSuccess()
  // will return true.
  Aws::SQS::Model::ReceiveMessageOutcome outcome(result);
  EXPECT_CALL(*mock_sqs_client, ReceiveMessage(::testing::_))
      .WillOnce(::testing::Return(outcome));

  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.queue_manager = &mock_message_service;
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<ChangeNotifier>> notifier =
      ChangeNotifier::Create(notifier_metadata);
  ASSERT_TRUE(notifier.status().ok());

  const absl::StatusOr<std::vector<std::string>> notifications =
      (*notifier)->GetNotifications(absl::Seconds(1), [] { return false; });
  ASSERT_TRUE(notifications.ok());
  EXPECT_THAT(*notifications,
              testing::UnorderedElementsAreArray({"mock message body"}));
}

}  // namespace
}  // namespace kv_server
