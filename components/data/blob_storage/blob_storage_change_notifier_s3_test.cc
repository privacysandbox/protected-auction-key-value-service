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

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "aws/sqs/SQSClient.h"
#include "aws/sqs/model/ReceiveMessageRequest.h"
#include "components/data/blob_storage/blob_storage_change_notifier.h"
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

// See this link for the JSON format of AWS S3 notifications to SQS that're
// parsing:
// https://docs.aws.amazon.com/AmazonS3/latest/userguide/notification-content-structure.html
class BlobStorageChangeNotifierS3Test : public ::testing::Test {
 protected:
  void SetUp() override {
    privacy_sandbox::server_common::telemetry::TelemetryConfig config_proto;
    config_proto.set_mode(
        privacy_sandbox::server_common::telemetry::TelemetryConfig::PROD);
    KVServerContextMap(
        privacy_sandbox::server_common::telemetry::BuildDependentConfig(
            config_proto));
  }
  void CreateRequiredSqsCallExpectations() {
    static const std::string mock_sqs_url("mock sqs url");
    EXPECT_CALL(mock_message_service_, IsSetupComplete)
        .WillOnce(::testing::Return(true));
    static const AwsQueueMetadata metadata = {.sqs_url = mock_sqs_url};
    EXPECT_CALL(mock_message_service_, GetQueueMetadata())
        .WillRepeatedly(::testing::Return(metadata));
  }

  void SetMockMessage(const std::string& mock_message, MockSqsClient& client) {
    Aws::SQS::Model::ReceiveMessageResult result;
    Aws::SQS::Model::Message message;
    message.SetBody(mock_message);
    result.AddMessages(message);
    // Because we populate the Outcome with a Result that means that IsSuccess()
    // will return true.
    Aws::SQS::Model::ReceiveMessageOutcome outcome(result);
    EXPECT_CALL(client, ReceiveMessage(::testing::_))
        .WillOnce(::testing::Return(outcome));
  }

  MockMessageService mock_message_service_;

 private:
  PlatformInitializer initializer_;
};

TEST_F(BlobStorageChangeNotifierS3Test, AwsSqsUnavailable) {
  CreateRequiredSqsCallExpectations();

  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.queue_manager = &mock_message_service_;
  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  // A default ReceiveMessageOutcome will be returned for calls to
  // mock_sqs_client.ReceiveMessage(_).
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<BlobStorageChangeNotifier>> notifier =
      BlobStorageChangeNotifier::Create(notifier_metadata);
  ASSERT_TRUE(notifier.status().ok());

  const absl::StatusOr<std::vector<std::string>> notifications =
      (*notifier)->GetNotifications(absl::Seconds(1), [] { return false; });
  EXPECT_EQ(::absl::StatusCode::kUnavailable, notifications.status().code());
}

TEST_F(BlobStorageChangeNotifierS3Test, InvalidJsonMessage) {
  CreateRequiredSqsCallExpectations();

  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  SetMockMessage("this is not valid json", *mock_sqs_client);

  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.queue_manager = &mock_message_service_;
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<BlobStorageChangeNotifier>> notifier =
      BlobStorageChangeNotifier::Create(notifier_metadata);
  ASSERT_TRUE(notifier.status().ok());

  const absl::StatusOr<std::vector<std::string>> notifications =
      (*notifier)->GetNotifications(absl::Seconds(1), [] { return false; });
  ASSERT_TRUE(notifications.ok());
  // The invalid json message is dropped.
  EXPECT_EQ(0, notifications->size());
}

TEST_F(BlobStorageChangeNotifierS3Test, JsonHasNoMessageObject) {
  CreateRequiredSqsCallExpectations();

  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  SetMockMessage("{}", *mock_sqs_client);

  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.queue_manager = &mock_message_service_;
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<BlobStorageChangeNotifier>> notifier =
      BlobStorageChangeNotifier::Create(notifier_metadata);
  ASSERT_TRUE(notifier.status().ok());

  const absl::StatusOr<std::vector<std::string>> notifications =
      (*notifier)->GetNotifications(absl::Seconds(1), [] { return false; });
  ASSERT_TRUE(notifications.ok());
  // The invalid json message is dropped.
  EXPECT_EQ(0, notifications->size());
}

TEST_F(BlobStorageChangeNotifierS3Test, MessageObjectIsNotAString) {
  CreateRequiredSqsCallExpectations();

  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  SetMockMessage(R"({
   "Message": {}
  })",
                 *mock_sqs_client);

  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.queue_manager = &mock_message_service_;
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<BlobStorageChangeNotifier>> notifier =
      BlobStorageChangeNotifier::Create(notifier_metadata);
  ASSERT_TRUE(notifier.status().ok());

  const absl::StatusOr<std::vector<std::string>> notifications =
      (*notifier)->GetNotifications(absl::Seconds(1), [] { return false; });
  ASSERT_TRUE(notifications.ok());
  // The invalid json message is dropped.
  EXPECT_EQ(0, notifications->size());
}

TEST_F(BlobStorageChangeNotifierS3Test, RecordsIsNotAList) {
  CreateRequiredSqsCallExpectations();

  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  SetMockMessage(R"({
   "Message": "{\"Records\": {} }"
  })",
                 *mock_sqs_client);

  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.queue_manager = &mock_message_service_;
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<BlobStorageChangeNotifier>> notifier =
      BlobStorageChangeNotifier::Create(notifier_metadata);
  ASSERT_TRUE(notifier.status().ok());

  const absl::StatusOr<std::vector<std::string>> notifications =
      (*notifier)->GetNotifications(absl::Seconds(1), [] { return false; });
  ASSERT_TRUE(notifications.ok());
  // The invalid json message is dropped.
  EXPECT_EQ(0, notifications->size());
}

TEST_F(BlobStorageChangeNotifierS3Test, NoS3RecordPresent) {
  CreateRequiredSqsCallExpectations();

  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  SetMockMessage(R"({
   "Message": "{\"Records\":[]}"
  })",
                 *mock_sqs_client);

  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.queue_manager = &mock_message_service_;
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<BlobStorageChangeNotifier>> notifier =
      BlobStorageChangeNotifier::Create(notifier_metadata);
  ASSERT_TRUE(notifier.status().ok());

  const absl::StatusOr<std::vector<std::string>> notifications =
      (*notifier)->GetNotifications(absl::Seconds(1), [] { return false; });
  ASSERT_TRUE(notifications.ok());
  // The invalid json message is dropped.
  EXPECT_EQ(0, notifications->size());
}

TEST_F(BlobStorageChangeNotifierS3Test, S3RecordIsNull) {
  CreateRequiredSqsCallExpectations();

  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  SetMockMessage(R"({
   "Message": "{\"Records\":[ {\"s3\":null}]}"
  })",
                 *mock_sqs_client);

  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.queue_manager = &mock_message_service_;
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<BlobStorageChangeNotifier>> notifier =
      BlobStorageChangeNotifier::Create(notifier_metadata);
  ASSERT_TRUE(notifier.status().ok());

  const absl::StatusOr<std::vector<std::string>> notifications =
      (*notifier)->GetNotifications(absl::Seconds(1), [] { return false; });
  ASSERT_TRUE(notifications.ok());
  // The invalid json message is dropped.
  EXPECT_EQ(0, notifications->size());
}

TEST_F(BlobStorageChangeNotifierS3Test, S3ObjectIsNull) {
  CreateRequiredSqsCallExpectations();

  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  SetMockMessage(R"({
   "Message": "{\"Records\":[ {\"s3\":{\"object\":null}}]}"
  })",
                 *mock_sqs_client);

  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.queue_manager = &mock_message_service_;
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<BlobStorageChangeNotifier>> notifier =
      BlobStorageChangeNotifier::Create(notifier_metadata);
  ASSERT_TRUE(notifier.status().ok());

  const absl::StatusOr<std::vector<std::string>> notifications =
      (*notifier)->GetNotifications(absl::Seconds(1), [] { return false; });
  ASSERT_TRUE(notifications.ok());
  // The invalid json message is dropped.
  EXPECT_EQ(0, notifications->size());
}

TEST_F(BlobStorageChangeNotifierS3Test, S3KeyIsNotAString) {
  CreateRequiredSqsCallExpectations();

  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  SetMockMessage(R"({
   "Message": "{\"Records\":[ {\"s3\":{\"object\":{\"key\":{}}}}}]}"
  })",
                 *mock_sqs_client);

  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.queue_manager = &mock_message_service_;
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<BlobStorageChangeNotifier>> notifier =
      BlobStorageChangeNotifier::Create(notifier_metadata);
  ASSERT_TRUE(notifier.status().ok());

  const absl::StatusOr<std::vector<std::string>> notifications =
      (*notifier)->GetNotifications(absl::Seconds(1), [] { return false; });
  ASSERT_TRUE(notifications.ok());
  // The invalid json message is dropped.
  EXPECT_EQ(0, notifications->size());
}

TEST_F(BlobStorageChangeNotifierS3Test, ValidJson) {
  CreateRequiredSqsCallExpectations();

  auto mock_sqs_client = std::make_unique<MockSqsClient>();
  SetMockMessage(R"({
   "Message": "{\"Records\":[ {\"s3\":{\"object\":{\"key\":\"HappyFace.jpg\"}}}]}"
  })",
                 *mock_sqs_client);

  AwsNotifierMetadata notifier_metadata;
  notifier_metadata.queue_manager = &mock_message_service_;
  notifier_metadata.only_for_testing_sqs_client_ = mock_sqs_client.release();

  absl::StatusOr<std::unique_ptr<BlobStorageChangeNotifier>> notifier =
      BlobStorageChangeNotifier::Create(notifier_metadata);
  ASSERT_TRUE(notifier.status().ok());

  const absl::StatusOr<std::vector<std::string>> notifications =
      (*notifier)->GetNotifications(absl::Seconds(1), [] { return false; });
  ASSERT_TRUE(notifications.ok());
  EXPECT_THAT(*notifications,
              testing::UnorderedElementsAreArray({"HappyFace.jpg"}));
}

}  // namespace
}  // namespace kv_server
