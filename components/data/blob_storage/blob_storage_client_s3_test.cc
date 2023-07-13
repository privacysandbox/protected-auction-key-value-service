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

#include <algorithm>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/status/statusor.h"
#include "aws/core/Aws.h"
#include "aws/s3/S3Client.h"
#include "aws/s3/model/DeleteObjectRequest.h"
#include "aws/s3/model/ListObjectsV2Request.h"
#include "aws/s3/model/Object.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/util/platform_initializer.h"
#include "gtest/gtest.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::MockMetricsRecorder;

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
  PlatformInitializer initializer_;
  privacy_sandbox::server_common::MockMetricsRecorder metrics_recorder_;
};

TEST_F(BlobStorageClientS3Test, DeleteBlobSucceeds) {
  auto mock_s3_client = std::make_unique<MockS3Client>();
  Aws::S3::Model::DeleteObjectResult result;  // An empty result means success.
  EXPECT_CALL(*mock_s3_client, DeleteObject(::testing::_))
      .WillOnce(::testing::Return(result));

  BlobStorageClient::ClientOptions options;
  options.s3_client_for_unit_testing_ = mock_s3_client.release();
  std::unique_ptr<BlobStorageClient> client =
      BlobStorageClient::Create(metrics_recorder_, options);
  ASSERT_TRUE(client != nullptr);

  BlobStorageClient::DataLocation location;
  EXPECT_TRUE(client->DeleteBlob(location).ok());
}

TEST_F(BlobStorageClientS3Test, DeleteBlobFails) {
  // By default an error is returned for calls to DeleteBlob().
  auto mock_s3_client = std::make_unique<MockS3Client>();

  BlobStorageClient::ClientOptions options;
  options.s3_client_for_unit_testing_ = mock_s3_client.release();
  std::unique_ptr<BlobStorageClient> client =
      BlobStorageClient::Create(metrics_recorder_, options);
  ASSERT_TRUE(client != nullptr);

  BlobStorageClient::DataLocation location;
  EXPECT_EQ(absl::StatusCode::kUnknown, client->DeleteBlob(location).code());
}

TEST_F(BlobStorageClientS3Test, ListBlobsSucceeds) {
  auto mock_s3_client = std::make_unique<MockS3Client>();
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

  BlobStorageClient::ClientOptions options;
  options.s3_client_for_unit_testing_ = mock_s3_client.release();
  std::unique_ptr<BlobStorageClient> client =
      BlobStorageClient::Create(metrics_recorder_, options);
  ASSERT_TRUE(client != nullptr);

  BlobStorageClient::DataLocation location;
  BlobStorageClient::ListOptions list_options;
  absl::StatusOr<std::vector<std::string>> response =
      client->ListBlobs(location, list_options);
  ASSERT_TRUE(response.ok());
  EXPECT_THAT(*response, testing::UnorderedElementsAreArray({"HappyFace.jpg"}));
}

TEST_F(BlobStorageClientS3Test, ListBlobsSucceedsWithContinuedRequests) {
  auto mock_s3_client = std::make_unique<MockS3Client>();
  // Set up two expected requests.  The first one is marked as truncated so that
  // the second one will happen.
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

  BlobStorageClient::ClientOptions options;
  options.s3_client_for_unit_testing_ = mock_s3_client.release();
  std::unique_ptr<BlobStorageClient> client =
      BlobStorageClient::Create(metrics_recorder_, options);
  ASSERT_TRUE(client != nullptr);

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
  auto mock_s3_client = std::make_unique<MockS3Client>();

  BlobStorageClient::ClientOptions options;
  options.s3_client_for_unit_testing_ = mock_s3_client.release();
  std::unique_ptr<BlobStorageClient> client =
      BlobStorageClient::Create(metrics_recorder_, options);
  ASSERT_TRUE(client != nullptr);

  BlobStorageClient::DataLocation location;
  BlobStorageClient::ListOptions list_options;
  EXPECT_EQ(absl::StatusCode::kUnknown,
            client->ListBlobs(location, list_options).status().code());
}

}  // namespace
}  // namespace kv_server
