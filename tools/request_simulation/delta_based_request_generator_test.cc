// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tools/request_simulation/delta_based_request_generator.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/synchronization/notification.h"
#include "components/data/common/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/constants.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/records_utils.h"
#include "public/test_util/mocks.h"
#include "src/telemetry/mocks.h"
#include "tools/request_simulation/request_generation_util.h"

using kv_server::BlobStorageChangeNotifier;
using kv_server::BlobStorageClient;
using kv_server::DataRecordStruct;
using kv_server::DeltaBasedRequestGenerator;
using kv_server::FilePrefix;
using kv_server::FileType;
using kv_server::KeyValueMutationRecordStruct;
using kv_server::KeyValueMutationType;
using kv_server::KVFileMetadata;
using kv_server::MessageQueue;
using kv_server::MockBlobReader;
using kv_server::MockBlobStorageChangeNotifier;
using kv_server::MockBlobStorageClient;
using kv_server::MockDeltaFileNotifier;
using kv_server::MockRealtimeNotifier;
using kv_server::MockStreamRecordReader;
using kv_server::MockStreamRecordReaderFactory;
using kv_server::Record;
using kv_server::ToDeltaFileName;
using kv_server::ToFlatBufferBuilder;
using kv_server::ToStringView;
using kv_server::UserDefinedFunctionsConfigStruct;
using kv_server::UserDefinedFunctionsLanguage;
using kv_server::Value;
using privacy_sandbox::server_common::MockMetricsRecorder;
using testing::_;
using testing::AllOf;
using testing::ByMove;
using testing::Field;
using testing::Pair;
using testing::Return;
using testing::ReturnRef;
using testing::UnorderedElementsAre;

namespace {

BlobStorageClient::DataLocation GetTestLocation(
    const std::string& basename = "") {
  static constexpr absl::string_view kBucket = "testbucket";
  return BlobStorageClient::DataLocation{.bucket = std::string(kBucket),
                                         .key = basename};
}

absl::AnyInvocable<std::string(std::string_view)> GetRequestGenFn() {
  return [](std::string_view key) {
    return kv_server::CreateKVDSPRequestBodyInJson(
        {std::string(key)}, "debug_token", "generation_id");
  };
}

class GenerateRequestsFromDeltaFilesTest : public ::testing::Test {
 protected:
  GenerateRequestsFromDeltaFilesTest()
      : message_queue_(10000),
        options_(DeltaBasedRequestGenerator::Options{
            .data_bucket = GetTestLocation().bucket,
            .message_queue = message_queue_,
            .blob_client = blob_client_,
            .delta_notifier = notifier_,
            .change_notifier = change_notifier_,
            .delta_stream_reader_factory = delta_stream_reader_factory_}) {}

  MockBlobStorageClient blob_client_;
  MockDeltaFileNotifier notifier_;
  MockBlobStorageChangeNotifier change_notifier_;
  MockStreamRecordReaderFactory delta_stream_reader_factory_;
  MessageQueue message_queue_;
  DeltaBasedRequestGenerator::Options options_;
};

TEST_F(GenerateRequestsFromDeltaFilesTest, LoadingDataFromDeltaFiles) {
  ON_CALL(blob_client_, ListBlobs)
      .WillByDefault(Return(std::vector<std::string>({})));
  DeltaBasedRequestGenerator request_generator(std::move(options_),
                                               std::move(GetRequestGenFn()));
  const std::string last_basename = "";
  EXPECT_CALL(notifier_,
              Start(_, GetTestLocation(),
                    UnorderedElementsAre(Pair("", last_basename)), _))
      .WillOnce([](BlobStorageChangeNotifier&, BlobStorageClient::DataLocation,
                   absl::flat_hash_map<std::string, std::string>,
                   std::function<void(const std::string& key)> callback) {
        callback(ToDeltaFileName(1).value());
        LOG(INFO) << "Notified 1 file";
        return absl::OkStatus();
      });
  EXPECT_CALL(notifier_, IsRunning).Times(1).WillOnce(Return(true));
  EXPECT_CALL(notifier_, Stop()).Times(1).WillOnce(Return(absl::OkStatus()));

  absl::Notification all_records_loaded;
  KVFileMetadata metadata;
  auto update_reader = std::make_unique<MockStreamRecordReader>();
  EXPECT_CALL(*update_reader, GetKVFileMetadata)
      .Times(1)
      .WillOnce(Return(metadata));
  EXPECT_CALL(*update_reader, ReadStreamRecords)
      .Times(1)
      .WillOnce(
          [](const std::function<absl::Status(std::string_view)>& callback) {
            callback(
                ToStringView(ToFlatBufferBuilder(DataRecordStruct{
                    .record =
                        KeyValueMutationRecordStruct{
                            KeyValueMutationType::Update, 3, "key", "value"}})))
                .IgnoreError();
            return absl::OkStatus();
          });
  EXPECT_CALL(delta_stream_reader_factory_, CreateConcurrentReader)
      .Times(1)
      .WillOnce(Return(ByMove(std::move(update_reader))));
  EXPECT_TRUE(request_generator.Start().ok());
  EXPECT_TRUE(request_generator.IsRunning());
  all_records_loaded.WaitForNotificationWithTimeout(absl::Seconds(2));
  EXPECT_TRUE(request_generator.Stop().ok());
  EXPECT_EQ(message_queue_.Size(), 1);
  auto message_in_the_queue = message_queue_.Pop();
  EXPECT_TRUE(message_in_the_queue.ok());
  EXPECT_EQ(message_in_the_queue.value(),
            kv_server::CreateKVDSPRequestBodyInJson(
                {std::string("key")}, "debug_token", "generation_id"));
}

}  // namespace
