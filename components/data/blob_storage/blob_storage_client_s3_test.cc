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

#include "components/data/blob_storage/blob_storage_client_s3.h"

#include <algorithm>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "aws/core/Aws.h"
#include "aws/s3/S3Client.h"
#include "aws/s3/model/DeleteObjectRequest.h"
#include "aws/s3/model/ListObjectsV2Request.h"
#include "aws/s3/model/Object.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/telemetry/server_definition.h"
#include "components/util/platform_initializer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

using testing::AllOf;
using testing::Property;

constexpr int64_t kMaxRangeBytes = 1024 * 1024 * 8;

class MockS3Client : public ::Aws::S3::S3Client {
 public:
  MOCK_METHOD(Aws::S3::Model::DeleteObjectOutcome, DeleteObject,
              (const Aws::S3::Model::DeleteObjectRequest& request),
              (const, override));

  MOCK_METHOD(Aws::S3::Model::ListObjectsV2Outcome, ListObjectsV2,
              (const Aws::S3::Model::ListObjectsV2Request& request),
              (const, override));
};

class BlobStorageClientS3Test : public ::testing::Test {
 protected:
  void SetUp() override {
    privacy_sandbox::server_common::telemetry::TelemetryConfig config_proto;
    config_proto.set_mode(
        privacy_sandbox::server_common::telemetry::TelemetryConfig::PROD);
    kv_server::KVServerContextMap(
        privacy_sandbox::server_common::telemetry::BuildDependentConfig(
            config_proto));
  }
  privacy_sandbox::server_common::log::NoOpContext no_op_context_;

 private:
  PlatformInitializer initializer_;
};

TEST_F(BlobStorageClientS3Test, DeleteBlobSucceeds) {
  auto mock_s3_client = std::make_shared<MockS3Client>();
  Aws::S3::Model::DeleteObjectResult result;  // An empty result means success
  EXPECT_CALL(*mock_s3_client, DeleteObject(::testing::_))
      .WillOnce(::testing::Return(result));

  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<S3BlobStorageClient>(mock_s3_client, kMaxRangeBytes,
                                            no_op_context_);
  BlobStorageClient::DataLocation location;
  EXPECT_TRUE(client->DeleteBlob(location).ok());
}

TEST_F(BlobStorageClientS3Test, DeleteBlobFails) {
  // By default an error is returned for calls to DeleteBlob().
  auto mock_s3_client = std::make_shared<MockS3Client>();

  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<S3BlobStorageClient>(mock_s3_client, kMaxRangeBytes,
                                            no_op_context_);
  BlobStorageClient::DataLocation location;
  EXPECT_EQ(absl::StatusCode::kUnknown, client->DeleteBlob(location).code());
}

TEST_F(BlobStorageClientS3Test, ListBlobsSucceeds) {
  auto mock_s3_client = std::make_shared<MockS3Client>();
  {
    Aws::S3::Model::ListObjectsV2Result
        result;  // An empty result means success.
    Aws::S3::Model::Object object_to_return;
    object_to_return.SetKey("HappyFace.jpg");
    Aws::Vector<Aws::S3::Model::Object> objects_to_return = {object_to_return};
    result.SetContents(objects_to_return);
    EXPECT_CALL(*mock_s3_client, ListObjectsV2(::testing::_))
        .WillOnce(::testing::Return(result));
  }

  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<S3BlobStorageClient>(mock_s3_client, kMaxRangeBytes,
                                            no_op_context_);
  BlobStorageClient::DataLocation location;
  BlobStorageClient::ListOptions list_options;
  absl::StatusOr<std::vector<std::string>> response =
      client->ListBlobs(location, list_options);
  ASSERT_TRUE(response.ok());
  EXPECT_THAT(*response, testing::UnorderedElementsAreArray({"HappyFace.jpg"}));
}

TEST_F(BlobStorageClientS3Test, ListBlobsSucceedsWithContinuedRequests) {
  auto mock_s3_client = std::make_shared<MockS3Client>();
  // Set up two expected requests.  The first one is marked as truncated so
  // that the second one will happen.
  ::testing::InSequence in_sequence;
  {
    Aws::S3::Model::ListObjectsV2Result
        result;  // An empty result means success.
    result.SetIsTruncated(true);
    Aws::S3::Model::Object object_to_return;
    object_to_return.SetKey("SomewhatHappyFace.jpg");
    Aws::Vector<Aws::S3::Model::Object> objects_to_return = {object_to_return};
    result.SetContents(objects_to_return);
    EXPECT_CALL(*mock_s3_client, ListObjectsV2(::testing::_))
        .WillOnce(::testing::Return(result));
  }
  {
    Aws::S3::Model::ListObjectsV2Result
        result;  // An empty result means success.
    Aws::S3::Model::Object object_to_return;
    object_to_return.SetKey("VeryHappyFace.jpg");
    Aws::Vector<Aws::S3::Model::Object> objects_to_return = {object_to_return};
    result.SetContents(objects_to_return);
    EXPECT_CALL(*mock_s3_client, ListObjectsV2(::testing::_))
        .WillOnce(::testing::Return(result));
  }

  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<S3BlobStorageClient>(mock_s3_client, kMaxRangeBytes,
                                            no_op_context_);
  BlobStorageClient::DataLocation location;
  BlobStorageClient::ListOptions list_options;
  absl::StatusOr<std::vector<std::string>> response =
      client->ListBlobs(location, list_options);
  ASSERT_TRUE(response.ok());
  EXPECT_THAT(*response, testing::UnorderedElementsAreArray(
                             {"SomewhatHappyFace.jpg", "VeryHappyFace.jpg"}));
}

TEST_F(BlobStorageClientS3Test, ListBlobsFails) {
  // By default an error is returned for calls to ListObjectsV2().
  auto mock_s3_client = std::make_shared<MockS3Client>();

  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<S3BlobStorageClient>(mock_s3_client, kMaxRangeBytes,
                                            no_op_context_);
  BlobStorageClient::DataLocation location;
  BlobStorageClient::ListOptions list_options;
  EXPECT_EQ(absl::StatusCode::kUnknown,
            client->ListBlobs(location, list_options).status().code());
}

TEST_F(BlobStorageClientS3Test, DeleteBlobWithPrefixSucceeds) {
  auto mock_s3_client = std::make_shared<MockS3Client>();
  Aws::S3::Model::DeleteObjectResult result;  // An empty result means success
  EXPECT_CALL(
      *mock_s3_client,
      DeleteObject(::testing::AllOf(
          testing::Property(&Aws::S3::Model::DeleteObjectRequest::GetBucket,
                            "bucket"),
          testing::Property(&Aws::S3::Model::DeleteObjectRequest::GetKey,
                            "prefix/object"))))
      .WillOnce(::testing::Return(result));
  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<S3BlobStorageClient>(mock_s3_client, kMaxRangeBytes,
                                            no_op_context_);
  BlobStorageClient::DataLocation location{
      .bucket = "bucket",
      .prefix = "prefix",
      .key = "object",
  };
  EXPECT_TRUE(client->DeleteBlob(location).ok());
}

TEST_F(BlobStorageClientS3Test, ListBlobsWithPrefixSucceeds) {
  auto mock_s3_client = std::make_shared<MockS3Client>();
  {
    Aws::S3::Model::ListObjectsV2Result
        result;  // An empty result means success.
    Aws::S3::Model::Object object_to_return;
    object_to_return.SetKey("directory1/DELTA_1699834075511696");
    Aws::Vector<Aws::S3::Model::Object> objects_to_return = {object_to_return};
    result.SetContents(objects_to_return);
    EXPECT_CALL(
        *mock_s3_client,
        ListObjectsV2(
            AllOf(Property(&Aws::S3::Model::ListObjectsV2Request::GetBucket,
                           "bucket"),
                  Property(&Aws::S3::Model::ListObjectsV2Request::GetPrefix,
                           "directory1/DELTA"),
                  Property(&Aws::S3::Model::ListObjectsV2Request::GetStartAfter,
                           "directory1/DELTA_1699834075511695"))))
        .WillOnce(::testing::Return(result));
  }

  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<S3BlobStorageClient>(mock_s3_client, kMaxRangeBytes,
                                            no_op_context_);
  BlobStorageClient::DataLocation location{
      .bucket = "bucket",
      .prefix = "directory1",
  };
  BlobStorageClient::ListOptions list_options{
      .prefix = "DELTA",
      .start_after = "DELTA_1699834075511695",
  };
  absl::StatusOr<std::vector<std::string>> response =
      client->ListBlobs(location, list_options);
  ASSERT_TRUE(response.ok());
  EXPECT_THAT(*response,
              testing::UnorderedElementsAreArray({"DELTA_1699834075511696"}));
}

}  // namespace
}  // namespace kv_server
