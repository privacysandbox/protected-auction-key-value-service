/*
 * Copyright 2025 Google LLC
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

#include "public/data_loading/readers/avro_delta_record_stream_reader.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/data_loading/record_utils.h"
#include "public/data_loading/writers/avro_delta_record_stream_writer.h"
#include "public/test_util/data_record.h"
#include "public/test_util/proto_matcher.h"

namespace kv_server {
namespace {

TEST(AvroDeltaRecordStreamReaderTest, KVRecord_ValidateReadingRecords) {
  std::stringstream string_stream;
  auto record_writer = AvroDeltaRecordStreamWriter<std::stringstream>::Create(
      string_stream, DeltaRecordWriter::Options{.metadata = KVFileMetadata()});
  ASSERT_TRUE(record_writer.ok());

  DataRecordT expected =
      GetNativeDataRecord(GetKVMutationRecord(GetSimpleStringValue()));
  ASSERT_TRUE((*record_writer)->WriteRecord(expected).ok());
  (*record_writer)->Close();
  AvroDeltaRecordStreamReader record_reader(string_stream);
  ASSERT_TRUE(record_reader
                  .ReadRecords([&expected](const DataRecord& data_record) {
                    EXPECT_EQ(*data_record.UnPack(), expected);
                    return absl::OkStatus();
                  })
                  .ok());
}

TEST(AvroDeltaRecordStreamReaderTest, KVRecord_ReadMetadata) {
  std::stringstream string_stream;
  KVFileMetadata kv_metadata;
  kv_metadata.mutable_sharding_metadata()->set_shard_num(17);
  auto record_writer = AvroDeltaRecordStreamWriter<std::stringstream>::Create(
      string_stream, DeltaRecordWriter::Options{.metadata = kv_metadata});
  ASSERT_TRUE(record_writer.ok());

  DataRecordT expected =
      GetNativeDataRecord(GetKVMutationRecord(GetSimpleStringValue()));
  ASSERT_TRUE((*record_writer)->WriteRecord(expected).ok());
  (*record_writer)->Close();
  AvroDeltaRecordStreamReader record_reader(string_stream);
  auto actual_metadata = record_reader.ReadMetadata();
  ASSERT_TRUE(actual_metadata.ok()) << actual_metadata.status();
  EXPECT_THAT(*actual_metadata, EqualsProto(kv_metadata));

  ASSERT_TRUE(record_reader
                  .ReadRecords([&expected](const DataRecord& data_record) {
                    EXPECT_EQ(*data_record.UnPack(), expected);
                    return absl::OkStatus();
                  })
                  .ok());
}

TEST(AvroDeltaRecordStreamReaderTest,
     KVRecord_ValidateReadingRecordCallsRecordCallback) {
  std::stringstream string_stream;
  auto record_writer = AvroDeltaRecordStreamWriter<std::stringstream>::Create(
      string_stream, DeltaRecordWriter::Options{.metadata = KVFileMetadata()});
  ASSERT_TRUE(record_writer.ok());

  DataRecordT expected =
      GetNativeDataRecord(GetKVMutationRecord(GetSimpleStringValue()));
  ASSERT_TRUE((*record_writer)->WriteRecord(expected).ok());
  ASSERT_TRUE((*record_writer)->WriteRecord(expected).ok());
  ASSERT_TRUE((*record_writer)->WriteRecord(expected).ok());
  ASSERT_TRUE((*record_writer)->WriteRecord(expected).ok());
  (*record_writer)->Close();
  AvroDeltaRecordStreamReader record_reader(string_stream);
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(4)
      .WillRepeatedly([&expected](const DataRecord& data_record) {
        EXPECT_EQ(*data_record.UnPack(), expected);
        return absl::OkStatus();
      });
  ASSERT_TRUE(record_reader.ReadRecords(record_callback.AsStdFunction()).ok());
}

TEST(AvroDeltaRecordStreamReaderTest, UdfConfig_ValidateReadingRecords) {
  std::stringstream string_stream;
  auto record_writer = AvroDeltaRecordStreamWriter<std::stringstream>::Create(
      string_stream, DeltaRecordWriter::Options{.metadata = KVFileMetadata()});
  ASSERT_TRUE(record_writer.ok());

  DataRecordT expected = GetNativeDataRecord(GetUserDefinedFunctionsConfig());
  ASSERT_TRUE((*record_writer)->WriteRecord(expected).ok());
  (*record_writer)->Close();
  AvroDeltaRecordStreamReader record_reader(string_stream);
  ASSERT_TRUE(record_reader
                  .ReadRecords([&expected](const DataRecord& data_record) {
                    EXPECT_EQ(*data_record.UnPack(), expected);
                    return absl::OkStatus();
                  })
                  .ok());
}

TEST(AvroDeltaRecordStreamReaderTest,
     UdfConfig_ValidateReadingRecordCallsRecordCallback) {
  std::stringstream string_stream;
  auto record_writer = AvroDeltaRecordStreamWriter<std::stringstream>::Create(
      string_stream, DeltaRecordWriter::Options{.metadata = KVFileMetadata()});
  ASSERT_TRUE(record_writer.ok());

  DataRecordT expected = GetNativeDataRecord(GetUserDefinedFunctionsConfig());
  ASSERT_TRUE((*record_writer)->WriteRecord(expected).ok());
  ASSERT_TRUE((*record_writer)->WriteRecord(expected).ok());
  ASSERT_TRUE((*record_writer)->WriteRecord(expected).ok());
  ASSERT_TRUE((*record_writer)->WriteRecord(expected).ok());
  (*record_writer)->Close();
  AvroDeltaRecordStreamReader record_reader(string_stream);
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(4)
      .WillRepeatedly([&expected](const DataRecord& data_record) {
        EXPECT_EQ(*data_record.UnPack(), expected);
        return absl::OkStatus();
      });
  ASSERT_TRUE(record_reader.ReadRecords(record_callback.AsStdFunction()).ok());
}

}  // namespace
}  // namespace kv_server
