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

#include "components/data/blob_storage/blob_storage_client_local.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <streambuf>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/status/statusor.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

class LocalBlobStorageClientTest : public ::testing::Test {
 protected:
  privacy_sandbox::server_common::log::NoOpContext no_op_context_;
};

void CreateSubDir(std::string_view subdir_name) {
  std::filesystem::create_directory(
      std::filesystem::path(::testing::TempDir()) / subdir_name);
}

void CreateFileInTmpDir(const std::string& filename) {
  const std::filesystem::path path =
      std::filesystem::path(::testing::TempDir()) / filename;
  std::ofstream file(path);
  file << "arbitrary file contents";
}

TEST_F(LocalBlobStorageClientTest, ListNotFoundDirectory) {
  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<FileBlobStorageClient>(no_op_context_);

  BlobStorageClient::DataLocation location;
  location.bucket = "this is not a valid directory path";

  kv_server::BlobStorageClient::ListOptions options;
  EXPECT_EQ(absl::StatusCode::kInternal,
            client->ListBlobs(location, options).status().code());
}

TEST_F(LocalBlobStorageClientTest, ListEmptyDirectory) {
  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<FileBlobStorageClient>(no_op_context_);

  BlobStorageClient::DataLocation location;
  // Directory contains no files by default.
  location.bucket = ::testing::TempDir();

  kv_server::BlobStorageClient::ListOptions options;
  const auto status_or = client->ListBlobs(location, options);
  EXPECT_TRUE(status_or.value().empty());
}

TEST_F(LocalBlobStorageClientTest, ListDirectoryWithFile) {
  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<FileBlobStorageClient>(no_op_context_);

  CreateFileInTmpDir("a");
  BlobStorageClient::DataLocation location;
  location.bucket = ::testing::TempDir();

  kv_server::BlobStorageClient::ListOptions options;
  const auto status_or = client->ListBlobs(location, options);
  ASSERT_TRUE(status_or.status().ok());
  EXPECT_EQ(*status_or, std::vector<std::string>{"a"});
}

TEST_F(LocalBlobStorageClientTest, DeleteNotFoundBlob) {
  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<FileBlobStorageClient>(no_op_context_);

  BlobStorageClient::DataLocation location;
  location.bucket = "this is not a valid directory path";
  location.key = "this is not a valid key";

  EXPECT_EQ(absl::StatusCode::kInternal, client->DeleteBlob(location).code());
}

TEST_F(LocalBlobStorageClientTest, DeleteBlob) {
  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<FileBlobStorageClient>(no_op_context_);

  BlobStorageClient::DataLocation location;
  location.bucket = ::testing::TempDir();
  location.key = "a";
  CreateFileInTmpDir("a");

  EXPECT_EQ(absl::StatusCode::kOk, client->DeleteBlob(location).code());
}

TEST_F(LocalBlobStorageClientTest, PutBlob) {
  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<FileBlobStorageClient>(no_op_context_);

  BlobStorageClient::DataLocation from;
  from.bucket = ::testing::TempDir();
  from.key = "a";
  CreateFileInTmpDir("a");
  auto from_blob_reader = client->GetBlobReader(from);

  BlobStorageClient::DataLocation to;
  to.bucket = ::testing::TempDir();
  to.key = "b";
  CreateFileInTmpDir("b");

  EXPECT_EQ(absl::StatusCode::kOk,
            client->PutBlob(*from_blob_reader, to).code());
}

TEST_F(LocalBlobStorageClientTest, DeleteBlobWithPrefix) {
  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<FileBlobStorageClient>(no_op_context_);
  CreateSubDir("prefix");
  BlobStorageClient::DataLocation location{
      .bucket = ::testing::TempDir(),
      .prefix = "prefix",
      .key = "object",
  };
  CreateFileInTmpDir("prefix/object");
  auto status = client->DeleteBlob(location);
  EXPECT_TRUE(status.ok()) << status;
  location.key = "non-existent-object";
  status = client->DeleteBlob(location);
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInternal) << status;
}

TEST_F(LocalBlobStorageClientTest, ListSubDirectoryWithFiles) {
  std::unique_ptr<BlobStorageClient> client =
      std::make_unique<FileBlobStorageClient>(no_op_context_);
  CreateSubDir("prefix");
  CreateFileInTmpDir("prefix/object1");
  CreateFileInTmpDir("prefix/object2");
  CreateFileInTmpDir("prefix/object3");
  BlobStorageClient::DataLocation location{
      .bucket = ::testing::TempDir(),
      .prefix = "prefix",
  };
  kv_server::BlobStorageClient::ListOptions options;
  auto status_or = client->ListBlobs(location, options);
  ASSERT_TRUE(status_or.ok()) << status_or.status();
  EXPECT_EQ(*status_or,
            std::vector<std::string>({"object1", "object2", "object3"}));
}

// TODO(237669491): Add tests here

}  // namespace
}  // namespace kv_server
