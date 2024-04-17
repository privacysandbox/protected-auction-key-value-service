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

#include "components/data/blob_storage/blob_storage_client_gcp.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <streambuf>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/status/statusor.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/telemetry/server_definition.h"
#include "components/util/platform_initializer.h"
#include "gmock/gmock.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

using ::google::cloud::storage::testing::canonical_errors::TransientError;
using testing::AllOf;
using testing::Property;

class GcpBlobStorageClientTest : public ::testing::Test {
 protected:
  PlatformInitializer initializer_;
  privacy_sandbox::server_common::log::NoOpContext no_op_context_;
  void SetUp() override {
    privacy_sandbox::server_common::telemetry::TelemetryConfig config_proto;
    config_proto.set_mode(
        privacy_sandbox::server_common::telemetry::TelemetryConfig::PROD);
    kv_server::KVServerContextMap(
        privacy_sandbox::server_common::telemetry::BuildDependentConfig(
            config_proto));
  }
};

TEST_F(GcpBlobStorageClientTest, DeleteBlobSucceeds) {
  namespace gcs = google::cloud::storage;

  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  std::unique_ptr<gcs::Client> mock_client = std::make_unique<gcs::Client>(
      gcs::internal::ClientImplDetails::CreateWithoutDecorations(mock));

  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(testing::Return(
          google::cloud::make_status_or(gcs::internal::EmptyResponse{})));

  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<GcpBlobStorageClient>(std::move(mock_client),
                                             no_op_context_);
  BlobStorageClient::DataLocation location;
  location.bucket = "test_bucket";
  location.key = "test_object";
  EXPECT_TRUE(client->DeleteBlob(location).ok());
}

TEST_F(GcpBlobStorageClientTest, DeleteBlobFails) {
  namespace gcs = google::cloud::storage;

  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  std::unique_ptr<gcs::Client> mock_client = std::make_unique<gcs::Client>(
      gcs::internal::ClientImplDetails::CreateWithoutDecorations(mock));

  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(testing::Return(google::cloud::Status(
          google::cloud::StatusCode::kPermissionDenied, "uh-oh")));

  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<GcpBlobStorageClient>(std::move(mock_client),
                                             no_op_context_);
  BlobStorageClient::DataLocation location;
  location.bucket = "test_bucket";
  location.key = "test_object";
  EXPECT_FALSE(client->DeleteBlob(location).ok());
}

TEST_F(GcpBlobStorageClientTest, ListBlobSucceeds) {
  namespace gcs = google::cloud::storage;

  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  std::unique_ptr<gcs::Client> mock_client =
      std::make_unique<gcs::Client>(gcs::testing::ClientFromMock(mock));

  gcs::internal::ListObjectsResponse response;
  response.items.push_back(
      gcs::ObjectMetadata{}.set_bucket("test_bucket").set_name("DELTAccc"));
  response.items.push_back(
      gcs::ObjectMetadata{}.set_bucket("test_bucket").set_name("DELTAbbb"));
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce(testing::Return(TransientError()))
      .WillOnce(testing::Return(response));

  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<GcpBlobStorageClient>(std::move(mock_client),
                                             no_op_context_);
  BlobStorageClient::DataLocation location;
  location.bucket = "test_bucket";
  location.key = "test_object";
  BlobStorageClient::ListOptions list_options;
  list_options.prefix = "DELTA";
  list_options.start_after = "DELTAaaa";
  auto my_blobs = client->ListBlobs(location, list_options);
  EXPECT_TRUE(my_blobs.ok());
  std::vector<std::string> expected_keys{"DELTAbbb", "DELTAccc"};
  EXPECT_EQ(*my_blobs, expected_keys);
}

TEST_F(GcpBlobStorageClientTest, ListBlobWithNonInclusiveStartAfter) {
  namespace gcs = google::cloud::storage;

  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  std::unique_ptr<gcs::Client> mock_client =
      std::make_unique<gcs::Client>(gcs::testing::ClientFromMock(mock));

  gcs::internal::ListObjectsResponse response;
  response.items.push_back(
      gcs::ObjectMetadata{}.set_bucket("test_bucket").set_name("DELTAbbb"));
  response.items.push_back(
      gcs::ObjectMetadata{}.set_bucket("test_bucket").set_name("DELTAccc"));
  response.items.push_back(
      gcs::ObjectMetadata{}.set_bucket("test_bucket").set_name("DELTAddd"));
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce(testing::Return(TransientError()))
      .WillOnce(testing::Return(response));

  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<GcpBlobStorageClient>(std::move(mock_client),
                                             no_op_context_);
  BlobStorageClient::DataLocation location;
  location.bucket = "test_bucket";
  location.key = "test_object";
  BlobStorageClient::ListOptions list_options;
  list_options.prefix = "DELTA";
  list_options.start_after = "DELTAbbb";
  auto my_blobs = client->ListBlobs(location, list_options);
  EXPECT_TRUE(my_blobs.ok());
  std::vector<std::string> expected_keys{"DELTAccc", "DELTAddd"};
  EXPECT_THAT(*my_blobs, expected_keys);
}

TEST_F(GcpBlobStorageClientTest, ListBlobWithNoNewObject) {
  namespace gcs = google::cloud::storage;

  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  std::unique_ptr<gcs::Client> mock_client =
      std::make_unique<gcs::Client>(gcs::testing::ClientFromMock(mock));

  gcs::internal::ListObjectsResponse response;
  response.items.push_back(
      gcs::ObjectMetadata{}.set_bucket("test_bucket").set_name("DELTAccc"));
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce(testing::Return(TransientError()))
      .WillOnce(testing::Return(response));

  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<GcpBlobStorageClient>(std::move(mock_client),
                                             no_op_context_);
  BlobStorageClient::DataLocation location;
  location.bucket = "test_bucket";
  location.key = "test_object";
  BlobStorageClient::ListOptions list_options;
  list_options.prefix = "DELTA";
  list_options.start_after = "DELTAccc";
  auto my_blobs = client->ListBlobs(location, list_options);
  EXPECT_TRUE(my_blobs.ok());
  EXPECT_TRUE(my_blobs->begin() == my_blobs->end());
}

TEST_F(GcpBlobStorageClientTest, DeleteBlobWithPrefixSucceeds) {
  namespace gcs = google::cloud::storage;
  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  std::unique_ptr<gcs::Client> mock_client = std::make_unique<gcs::Client>(
      gcs::internal::ClientImplDetails::CreateWithoutDecorations(mock));
  EXPECT_CALL(*mock,
              DeleteObject(AllOf(
                  Property(&gcs::internal::DeleteObjectRequest::bucket_name,
                           "test_bucket"),
                  Property(&gcs::internal::DeleteObjectRequest::object_name,
                           "test_prefix/test_object"))))
      .WillOnce(testing::Return(
          google::cloud::make_status_or(gcs::internal::EmptyResponse{})));

  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<GcpBlobStorageClient>(std::move(mock_client),
                                             no_op_context_);
  BlobStorageClient::DataLocation location{
      .bucket = "test_bucket",
      .prefix = "test_prefix",
      .key = "test_object",
  };
  EXPECT_TRUE(client->DeleteBlob(location).ok());
}

}  // namespace
}  // namespace kv_server
