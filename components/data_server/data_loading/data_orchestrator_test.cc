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

#include "components/data_server/data_loading/data_orchestrator.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/synchronization/notification.h"
#include "components/data/mocks.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/mocks.h"
#include "glog/logging.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "public/constants.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/records_utils.h"
#include "public/test_util/mocks.h"
#include "public/test_util/proto_matcher.h"

using fledge::kv_server::BlobStorageChangeNotifier;
using fledge::kv_server::BlobStorageClient;
using fledge::kv_server::DataOrchestrator;
using fledge::kv_server::DeltaFileRecordStruct;
using fledge::kv_server::DeltaMutationType;
using fledge::kv_server::FilePrefix;
using fledge::kv_server::FileType;
using fledge::kv_server::FullKeyEq;
using fledge::kv_server::KeyNamespace;
using fledge::kv_server::KVFileMetadata;
using fledge::kv_server::MockBlobReader;
using fledge::kv_server::MockBlobStorageChangeNotifier;
using fledge::kv_server::MockBlobStorageClient;
using fledge::kv_server::MockCache;
using fledge::kv_server::MockDeltaFileNotifier;
using fledge::kv_server::MockShardedCache;
using fledge::kv_server::MockStreamRecordReader;
using fledge::kv_server::MockStreamRecordReaderFactory;
using fledge::kv_server::ToDeltaFileName;
using fledge::kv_server::ToStringView;
using testing::_;
using testing::AllOf;
using testing::ByMove;
using testing::Field;
using testing::Return;
using testing::ReturnRef;

namespace {

// using google::protobuf::TextFormat;

BlobStorageClient::DataLocation GetTestLocation(
    const std::string& basename = "") {
  static constexpr absl::string_view kBucket = "testbucket";
  return BlobStorageClient::DataLocation{.bucket = std::string(kBucket),
                                         .key = basename};
}

class DataOrchestratorTest : public ::testing::Test {
 protected:
  DataOrchestratorTest()
      : options_(DataOrchestrator::Options{
            .data_bucket = GetTestLocation().bucket,
            .cache = sharded_cache_,
            .blob_client = blob_client_,
            .delta_notifier = notifier_,
            .change_notifier = change_notifier_,
            .delta_stream_reader_factory = delta_stream_reader_factory_}) {}

  MockBlobStorageClient blob_client_;
  MockDeltaFileNotifier notifier_;
  MockBlobStorageChangeNotifier change_notifier_;
  MockStreamRecordReaderFactory delta_stream_reader_factory_;
  MockCache cache_;
  MockShardedCache sharded_cache_;
  DataOrchestrator::Options options_;
};

TEST_F(DataOrchestratorTest, InitCacheListRetriesOnFailure) {
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::DELTA>()))))
      .Times(1)
      .WillOnce(Return(absl::UnknownError("list failed")));

  EXPECT_EQ(DataOrchestrator::TryCreate(options_).status(),
            absl::UnknownError("list failed"));
}

TEST_F(DataOrchestratorTest, InitCacheNoFiles) {
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::DELTA>()))))
      .WillOnce(Return(std::vector<std::string>()));

  EXPECT_CALL(blob_client_, GetBlobReader).Times(0);

  EXPECT_TRUE(DataOrchestrator::TryCreate(options_).ok());
}

TEST_F(DataOrchestratorTest, InitCacheFilteroutInvalidFiles) {
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::DELTA>()))))
      .WillOnce(Return(std::vector<std::string>({"DELTA_01"})));

  EXPECT_CALL(blob_client_, GetBlobReader).Times(0);

  EXPECT_TRUE(DataOrchestrator::TryCreate(options_).ok());
}

TEST_F(DataOrchestratorTest, InitCacheSuccess) {
  const std::vector<std::string> fnames(
      {ToDeltaFileName(1).value(), ToDeltaFileName(2).value()});

  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::DELTA>()))))
      .WillOnce(Return(fnames));

  std::stringstream dummy_stream;
  for (const auto& basename : fnames) {
    auto blob_reader = std::make_unique<MockBlobReader>();
    ON_CALL(*blob_reader, Stream())
        .WillByDefault(
            [&dummy_stream]() -> std::istream& { return dummy_stream; });

    EXPECT_CALL(blob_client_, GetBlobReader(GetTestLocation(basename)))
        .WillOnce(Return(ByMove(std::move(blob_reader))));
  }

  KVFileMetadata metadata;
  metadata.set_key_namespace(KeyNamespace::KEYS);
  auto update_reader = std::make_unique<MockStreamRecordReader>();
  EXPECT_CALL(*update_reader, GetKVFileMetadata).Times(1).WillOnce([&metadata] {
    return metadata;
  });
  EXPECT_CALL(*update_reader, ReadStreamRecords)
      .Times(1)
      .WillOnce(
          [](const std::function<absl::Status(std::string_view)>& callback) {
            const auto fb = DeltaFileRecordStruct{DeltaMutationType::Update, 3,
                                                  "bar", "subkey", "bar value"}
                                .ToFlatBuffer();
            callback(ToStringView(fb)).IgnoreError();
            return absl::OkStatus();
          });
  auto delete_reader = std::make_unique<MockStreamRecordReader>();
  EXPECT_CALL(*delete_reader, GetKVFileMetadata).Times(1).WillOnce([&metadata] {
    return metadata;
  });
  EXPECT_CALL(*delete_reader, ReadStreamRecords)
      .Times(1)
      .WillOnce(
          [](const std::function<absl::Status(std::string_view)>& callback) {
            const auto fb = DeltaFileRecordStruct{DeltaMutationType::Delete, 3,
                                                  "bar", "subkey2"}
                                .ToFlatBuffer();
            callback(ToStringView(fb)).IgnoreError();
            return absl::OkStatus();
          });
  EXPECT_CALL(delta_stream_reader_factory_, CreateReader)
      .Times(2)
      .WillOnce(Return(ByMove(std::move(update_reader))))
      .WillOnce(Return(ByMove(std::move(delete_reader))));

  EXPECT_CALL(sharded_cache_, GetMutableCacheShard(KeyNamespace::KEYS))
      .Times(2)
      .WillRepeatedly(ReturnRef(cache_));
  EXPECT_CALL(cache_, UpdateKeyValue(FullKeyEq("bar", "subkey"), "bar value"))
      .Times(1);
  EXPECT_CALL(cache_, DeleteKey(FullKeyEq("bar", "subkey2"))).Times(1);

  auto maybe_orchestrator = DataOrchestrator::TryCreate(options_);
  ASSERT_TRUE(maybe_orchestrator.ok());

  const std::string last_basename = ToDeltaFileName(2).value();
  EXPECT_CALL(notifier_, StartNotify(_, GetTestLocation(), last_basename, _))
      .WillOnce(Return(absl::UnknownError("")));
  EXPECT_FALSE((*maybe_orchestrator)->Start().ok());
}

TEST_F(DataOrchestratorTest, StartLoading) {
  ON_CALL(blob_client_, ListBlobs)
      .WillByDefault(Return(std::vector<std::string>({})));
  auto maybe_orchestrator = DataOrchestrator::TryCreate(options_);
  ASSERT_TRUE(maybe_orchestrator.ok());
  auto orchestrator = std::move(maybe_orchestrator.value());

  const std::string last_basename = "";
  EXPECT_CALL(notifier_, StartNotify(_, GetTestLocation(), last_basename, _))
      .WillOnce([](BlobStorageChangeNotifier& change_notifier,
                   BlobStorageClient::DataLocation location,
                   std::string start_after,
                   std::function<void(const std::string& key)> callback) {
        callback(ToDeltaFileName(6).value());
        callback(ToDeltaFileName(7).value());
        LOG(INFO) << "Notified 2 files";
        return absl::OkStatus();
      });

  EXPECT_CALL(notifier_, IsRunning).Times(1).WillOnce(Return(true));
  EXPECT_CALL(notifier_, StopNotify())
      .Times(1)
      .WillOnce(Return(absl::OkStatus()));

  std::stringstream dummy_stream;
  for (const auto& basename :
       {ToDeltaFileName(6).value(), ToDeltaFileName(7).value()}) {
    EXPECT_CALL(blob_client_, GetBlobReader(GetTestLocation(basename)))
        .WillOnce([&dummy_stream] {
          auto blob_reader = std::make_unique<MockBlobReader>();
          ON_CALL(*blob_reader, Stream())
              .WillByDefault(
                  [&dummy_stream]() -> std::istream& { return dummy_stream; });
          return blob_reader;
        });
  }

  absl::Notification all_records_loaded;
  KVFileMetadata metadata;
  metadata.set_key_namespace(KeyNamespace::KEYS);
  auto update_reader = std::make_unique<MockStreamRecordReader>();
  EXPECT_CALL(*update_reader, GetKVFileMetadata).Times(1).WillOnce([&metadata] {
    return metadata;
  });
  EXPECT_CALL(*update_reader, ReadStreamRecords)
      .Times(1)
      .WillOnce(
          [](const std::function<absl::Status(std::string_view)>& callback) {
            const auto fb = DeltaFileRecordStruct{DeltaMutationType::Update, 3,
                                                  "bar", "subkey", "bar value"}
                                .ToFlatBuffer();
            callback(ToStringView(fb)).IgnoreError();
            return absl::OkStatus();
          });
  auto delete_reader = std::make_unique<MockStreamRecordReader>();
  EXPECT_CALL(*delete_reader, GetKVFileMetadata).Times(1).WillOnce([&metadata] {
    return metadata;
  });
  EXPECT_CALL(*delete_reader, ReadStreamRecords)
      .Times(1)
      .WillOnce(
          [&all_records_loaded](
              const std::function<absl::Status(std::string_view)>& callback) {
            const auto fb = DeltaFileRecordStruct{DeltaMutationType::Delete, 3,
                                                  "bar", "subkey2"}
                                .ToFlatBuffer();
            callback(ToStringView(fb)).IgnoreError();
            all_records_loaded.Notify();
            return absl::OkStatus();
          });
  EXPECT_CALL(delta_stream_reader_factory_, CreateReader)
      .Times(2)
      .WillOnce(Return(ByMove(std::move(update_reader))))
      .WillOnce(Return(ByMove(std::move(delete_reader))));

  EXPECT_CALL(sharded_cache_, GetMutableCacheShard(KeyNamespace::KEYS))
      .Times(2)
      .WillRepeatedly(ReturnRef(cache_));
  EXPECT_CALL(cache_, UpdateKeyValue(FullKeyEq("bar", "subkey"), "bar value"))
      .Times(1);
  EXPECT_CALL(cache_, DeleteKey(FullKeyEq("bar", "subkey2"))).Times(1);

  EXPECT_TRUE(orchestrator->Start().ok());
  LOG(INFO) << "Created ContinuouslyLoadNewData";
  all_records_loaded.WaitForNotificationWithTimeout(absl::Seconds(10));
}

}  // namespace
