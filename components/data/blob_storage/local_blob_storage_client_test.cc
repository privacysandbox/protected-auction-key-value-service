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

void CreateFileInTmpDir(const std::string& filename) {
  const std::filesystem::path path =
      std::filesystem::path(::testing::TempDir()) / filename;
  std::ofstream file(path);
  file << "arbitrary file contents";
}

TEST(LocalBlobStorageClientTest, ListNotFoundDirectory) {
  std::unique_ptr<BlobStorageClient> client = BlobStorageClient::Create();
  ASSERT_TRUE(client != nullptr);

  BlobStorageClient::DataLocation location;
  location.bucket = "this is not a valid directory path";

  kv_server::BlobStorageClient::ListOptions options;
  EXPECT_EQ(absl::StatusCode::kInternal,
            client->ListBlobs(location, options).status().code());
}

TEST(LocalBlobStorageClientTest, ListEmptyDirectory) {
  std::unique_ptr<BlobStorageClient> client = BlobStorageClient::Create();
  ASSERT_TRUE(client != nullptr);

  BlobStorageClient::DataLocation location;
  // Directory contains no files by default.
  location.bucket = ::testing::TempDir();

  kv_server::BlobStorageClient::ListOptions options;
  const auto status_or = client->ListBlobs(location, options);
  EXPECT_TRUE(status_or.value().empty());
}

TEST(LocalBlobStorageClientTest, ListDirectoryWithFile) {
  std::unique_ptr<BlobStorageClient> client = BlobStorageClient::Create();
  ASSERT_TRUE(client != nullptr);

  CreateFileInTmpDir("a");
  BlobStorageClient::DataLocation location;
  location.bucket = ::testing::TempDir();

  kv_server::BlobStorageClient::ListOptions options;
  const auto status_or = client->ListBlobs(location, options);
  ASSERT_TRUE(status_or.status().ok());
  EXPECT_EQ(*status_or, std::vector<std::string>{"a"});
}

TEST(LocalBlobStorageClientTest, DeleteNotFoundBlob) {
  std::unique_ptr<BlobStorageClient> client = BlobStorageClient::Create();
  ASSERT_TRUE(client != nullptr);

  BlobStorageClient::DataLocation location;
  location.bucket = "this is not a valid directory path";
  location.key = "this is not a valid key";

  EXPECT_EQ(absl::StatusCode::kInternal, client->DeleteBlob(location).code());
}

TEST(LocalBlobStorageClientTest, DeleteBlob) {
  std::unique_ptr<BlobStorageClient> client = BlobStorageClient::Create();
  ASSERT_TRUE(client != nullptr);

  BlobStorageClient::DataLocation location;
  location.bucket = ::testing::TempDir();
  location.key = "a";
  CreateFileInTmpDir("a");

  EXPECT_EQ(absl::StatusCode::kOk, client->DeleteBlob(location).code());
}

TEST(LocalBlobStorageClientTest, PutBlob) {
  std::unique_ptr<BlobStorageClient> client = BlobStorageClient::Create();
  ASSERT_TRUE(client != nullptr);

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

// TODO(237669491): Add tests here

}  // namespace
}  // namespace kv_server
