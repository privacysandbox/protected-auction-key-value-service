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
#include "components/data/common/mocks.h"
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
#include "src/cpp/telemetry/mocks.h"

using kv_server::BlobStorageChangeNotifier;
using kv_server::BlobStorageClient;
using kv_server::DataOrchestrator;
using kv_server::DeltaFileRecordStruct;
using kv_server::DeltaMutationType;
using kv_server::FilePrefix;
using kv_server::FileType;
using kv_server::KVFileMetadata;
using kv_server::MockBlobReader;
using kv_server::MockBlobStorageChangeNotifier;
using kv_server::MockBlobStorageClient;
using kv_server::MockCache;
using kv_server::MockDeltaFileNotifier;
using kv_server::MockDeltaFileRecordChangeNotifier;
using kv_server::MockRealtimeNotifier;
using kv_server::MockStreamRecordReader;
using kv_server::MockStreamRecordReaderFactory;
using kv_server::ToDeltaFileName;
using kv_server::ToSnapshotFileName;
using kv_server::ToStringView;
using privacy_sandbox::server_common::MockMetricsRecorder;
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
            .cache = cache_,
            .blob_client = blob_client_,
            .delta_notifier = notifier_,
            .change_notifier = change_notifier_,
            .delta_stream_reader_factory = delta_stream_reader_factory_,
            .realtime_options = realtime_options_}) {}

  MockBlobStorageClient blob_client_;
  MockDeltaFileNotifier notifier_;
  MockBlobStorageChangeNotifier change_notifier_;
  MockStreamRecordReaderFactory delta_stream_reader_factory_;
  MockCache cache_;
  std::vector<DataOrchestrator::RealtimeOptions> realtime_options_;
  DataOrchestrator::Options options_;
  MockMetricsRecorder metrics_recorder_;
};

TEST_F(DataOrchestratorTest, InitCacheListRetriesOnFailure) {
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::SNAPSHOT>()))))
      .Times(1)
      .WillOnce(Return(std::vector<std::string>()));
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::DELTA>()))))
      .Times(1)
      .WillOnce(Return(absl::UnknownError("list failed")));

  EXPECT_EQ(DataOrchestrator::TryCreate(options_, metrics_recorder_).status(),
            absl::UnknownError("list failed"));
}

TEST_F(DataOrchestratorTest, InitCacheListSnapshotsFailure) {
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::SNAPSHOT>()))))
      .Times(1)
      .WillOnce(Return(absl::UnknownError("list snapshots failed")));
  EXPECT_EQ(DataOrchestrator::TryCreate(options_, metrics_recorder_).status(),
            absl::UnknownError("list snapshots failed"));
}

TEST_F(DataOrchestratorTest, InitCacheNoFiles) {
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::SNAPSHOT>()))))
      .Times(1)
      .WillOnce(Return(std::vector<std::string>()));
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::DELTA>()))))
      .WillOnce(Return(std::vector<std::string>()));

  EXPECT_CALL(blob_client_, GetBlobReader).Times(0);

  EXPECT_TRUE(DataOrchestrator::TryCreate(options_, metrics_recorder_).ok());
}

TEST_F(DataOrchestratorTest, InitCacheFilteroutInvalidFiles) {
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::SNAPSHOT>()))))
      .Times(1)
      .WillOnce(Return(std::vector<std::string>()));
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::DELTA>()))))
      .WillOnce(Return(std::vector<std::string>({"DELTA_01"})));

  EXPECT_CALL(blob_client_, GetBlobReader).Times(0);

  EXPECT_TRUE(DataOrchestrator::TryCreate(options_, metrics_recorder_).ok());
}

TEST_F(DataOrchestratorTest, InitCacheFiltersDeltasUsingSnapshotEndingFile) {
  auto snapshot_name = ToSnapshotFileName(1);
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::SNAPSHOT>()))))
      .Times(1)
      .WillOnce(Return(std::vector<std::string>({*snapshot_name})));
  KVFileMetadata metadata;
  *metadata.mutable_snapshot()->mutable_starting_file() =
      ToDeltaFileName(1).value();
  *metadata.mutable_snapshot()->mutable_ending_delta_file() =
      ToDeltaFileName(5).value();
  auto record_reader1 = std::make_unique<MockStreamRecordReader>();
  EXPECT_CALL(*record_reader1, GetKVFileMetadata)
      .Times(1)
      .WillOnce(Return(metadata));
  auto record_reader2 = std::make_unique<MockStreamRecordReader>();
  EXPECT_CALL(delta_stream_reader_factory_, CreateConcurrentReader)
      .Times(2)
      .WillOnce(Return(ByMove(std::move(record_reader1))))
      .WillOnce(Return(ByMove(std::move(record_reader2))));

  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after,
                            ToDeltaFileName(5).value()),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::DELTA>()))))
      .WillOnce(Return(std::vector<std::string>()));
  EXPECT_TRUE(DataOrchestrator::TryCreate(options_, metrics_recorder_).ok());
}

TEST_F(DataOrchestratorTest, InitCacheSuccess) {
  const std::vector<std::string> fnames(
      {ToDeltaFileName(1).value(), ToDeltaFileName(2).value()});
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::SNAPSHOT>()))))
      .Times(1)
      .WillOnce(Return(std::vector<std::string>()));
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::DELTA>()))))
      .WillOnce(Return(fnames));

  KVFileMetadata metadata;
  auto update_reader = std::make_unique<MockStreamRecordReader>();
  EXPECT_CALL(*update_reader, ReadStreamRecords)
      .Times(1)
      .WillOnce(
          [](const std::function<absl::Status(std::string_view)>& callback) {
            const auto fb = DeltaFileRecordStruct{DeltaMutationType::Update, 3,
                                                  "bar", "bar value"}
                                .ToFlatBuffer();
            callback(ToStringView(fb)).IgnoreError();
            return absl::OkStatus();
          });
  auto delete_reader = std::make_unique<MockStreamRecordReader>();
  EXPECT_CALL(*delete_reader, ReadStreamRecords)
      .Times(1)
      .WillOnce(
          [](const std::function<absl::Status(std::string_view)>& callback) {
            const auto fb = DeltaFileRecordStruct{DeltaMutationType::Delete, 3,
                                                  "bar", "bar value"}
                                .ToFlatBuffer();
            callback(ToStringView(fb)).IgnoreError();
            return absl::OkStatus();
          });
  EXPECT_CALL(delta_stream_reader_factory_, CreateConcurrentReader)
      .Times(2)
      .WillOnce(Return(ByMove(std::move(update_reader))))
      .WillOnce(Return(ByMove(std::move(delete_reader))));

  EXPECT_CALL(cache_, UpdateKeyValue("bar", "bar value", 3)).Times(1);
  EXPECT_CALL(cache_, DeleteKey("bar", 3)).Times(1);
  EXPECT_CALL(cache_, RemoveDeletedKeys(3)).Times(2);

  auto maybe_orchestrator =
      DataOrchestrator::TryCreate(options_, metrics_recorder_);
  ASSERT_TRUE(maybe_orchestrator.ok());

  const std::string last_basename = ToDeltaFileName(2).value();
  EXPECT_CALL(notifier_, Start(_, GetTestLocation(), last_basename, _))
      .WillOnce(Return(absl::UnknownError("")));
  EXPECT_FALSE((*maybe_orchestrator)->Start().ok());
}

TEST_F(DataOrchestratorTest, StartLoading) {
  ON_CALL(blob_client_, ListBlobs)
      .WillByDefault(Return(std::vector<std::string>({})));
  auto maybe_orchestrator =
      DataOrchestrator::TryCreate(options_, metrics_recorder_);
  ASSERT_TRUE(maybe_orchestrator.ok());
  auto orchestrator = std::move(maybe_orchestrator.value());

  const std::string last_basename = "";
  EXPECT_CALL(notifier_, Start(_, GetTestLocation(), last_basename, _))
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
  EXPECT_CALL(notifier_, Stop()).Times(1).WillOnce(Return(absl::OkStatus()));

  absl::Notification all_records_loaded;
  KVFileMetadata metadata;
  auto update_reader = std::make_unique<MockStreamRecordReader>();
  EXPECT_CALL(*update_reader, ReadStreamRecords)
      .Times(1)
      .WillOnce(
          [](const std::function<absl::Status(std::string_view)>& callback) {
            const auto fb = DeltaFileRecordStruct{DeltaMutationType::Update, 3,
                                                  "bar", "bar value"}
                                .ToFlatBuffer();
            callback(ToStringView(fb)).IgnoreError();
            return absl::OkStatus();
          });
  auto delete_reader = std::make_unique<MockStreamRecordReader>();
  EXPECT_CALL(*delete_reader, ReadStreamRecords)
      .Times(1)
      .WillOnce(
          [&all_records_loaded](
              const std::function<absl::Status(std::string_view)>& callback) {
            const auto fb = DeltaFileRecordStruct{DeltaMutationType::Delete, 3,
                                                  "bar", "bar value"}
                                .ToFlatBuffer();
            callback(ToStringView(fb)).IgnoreError();
            all_records_loaded.Notify();
            return absl::OkStatus();
          });
  EXPECT_CALL(delta_stream_reader_factory_, CreateConcurrentReader)
      .Times(2)
      .WillOnce(Return(ByMove(std::move(update_reader))))
      .WillOnce(Return(ByMove(std::move(delete_reader))));

  EXPECT_CALL(cache_, UpdateKeyValue("bar", "bar value", 3)).Times(1);
  EXPECT_CALL(cache_, DeleteKey("bar", 3)).Times(1);
  EXPECT_CALL(cache_, RemoveDeletedKeys(3)).Times(2);

  EXPECT_TRUE(orchestrator->Start().ok());
  LOG(INFO) << "Created ContinuouslyLoadNewData";
  all_records_loaded.WaitForNotificationWithTimeout(absl::Seconds(10));
}

TEST_F(DataOrchestratorTest, CreateOrchestratorWithRealtimeDisabled) {
  ON_CALL(blob_client_, ListBlobs)
      .WillByDefault(Return(std::vector<std::string>({})));
  auto maybe_orchestrator =
      DataOrchestrator::TryCreate(options_, metrics_recorder_);
  ASSERT_TRUE(maybe_orchestrator.ok());
}

TEST_F(DataOrchestratorTest, InitCacheShardedSuccessSkipRecord) {
  testing::StrictMock<MockCache> strict_cache;

  const std::vector<std::string> fnames(
      {ToDeltaFileName(1).value(), ToDeltaFileName(2).value()});
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::SNAPSHOT>()))))
      .Times(1)
      .WillOnce(Return(std::vector<std::string>()));
  EXPECT_CALL(
      blob_client_,
      ListBlobs(GetTestLocation(),
                AllOf(Field(&BlobStorageClient::ListOptions::start_after, ""),
                      Field(&BlobStorageClient::ListOptions::prefix,
                            FilePrefix<FileType::DELTA>()))))
      .WillOnce(Return(fnames));

  KVFileMetadata metadata;
  auto update_reader = std::make_unique<MockStreamRecordReader>();
  EXPECT_CALL(*update_reader, ReadStreamRecords)
      .Times(1)
      .WillOnce(
          [](const std::function<absl::Status(std::string_view)>& callback) {
            // key: "shard2" -> shard num: 0
            const auto fb = DeltaFileRecordStruct{DeltaMutationType::Update, 3,
                                                  "shard1", "bar value"}
                                .ToFlatBuffer();
            callback(ToStringView(fb)).IgnoreError();
            return absl::OkStatus();
          });
  auto delete_reader = std::make_unique<MockStreamRecordReader>();
  EXPECT_CALL(*delete_reader, ReadStreamRecords)
      .Times(1)
      .WillOnce(
          [](const std::function<absl::Status(std::string_view)>& callback) {
            // key: "shard2" -> shard num: 1
            const auto fb = DeltaFileRecordStruct{DeltaMutationType::Delete, 3,
                                                  "shard2", "bar value"}
                                .ToFlatBuffer();
            callback(ToStringView(fb)).IgnoreError();
            return absl::OkStatus();
          });
  EXPECT_CALL(delta_stream_reader_factory_, CreateConcurrentReader)
      .Times(2)
      .WillOnce(Return(ByMove(std::move(update_reader))))
      .WillOnce(Return(ByMove(std::move(delete_reader))));

  EXPECT_CALL(strict_cache, RemoveDeletedKeys(0)).Times(1);
  EXPECT_CALL(strict_cache, DeleteKey("shard2", 3)).Times(1);
  EXPECT_CALL(strict_cache, RemoveDeletedKeys(3)).Times(1);

  auto sharded_options = DataOrchestrator::Options{
      .data_bucket = GetTestLocation().bucket,
      .cache = strict_cache,
      .blob_client = blob_client_,
      .delta_notifier = notifier_,
      .change_notifier = change_notifier_,
      .delta_stream_reader_factory = delta_stream_reader_factory_,
      .realtime_options = realtime_options_,
      .num_shards = 2,
      .shard_num = 1};

  auto maybe_orchestrator =
      DataOrchestrator::TryCreate(sharded_options, metrics_recorder_);
  ASSERT_TRUE(maybe_orchestrator.ok());
}

}  // namespace
