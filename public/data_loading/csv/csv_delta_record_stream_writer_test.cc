/*
 * Copyright 2022 Google LLC
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

#include "public/data_loading/csv/csv_delta_record_stream_writer.h"

#include <sstream>

#include "gtest/gtest.h"
#include "public/data_loading/csv/csv_delta_record_stream_reader.h"
#include "public/data_loading/records_utils.h"
#include "public/test_util/data_record.h"

namespace kv_server {
namespace {

TEST(CsvDeltaRecordStreamWriterTest,
     ValidateWritingCsvRecord_KVMutation_StringValue_Success) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);

  DataRecordT expected =
      GetNativeDataRecord(GetKVMutationRecord(GetSimpleStringValue()));
  const auto write_record_status = record_writer.WriteRecord(expected);
  EXPECT_TRUE(write_record_status.ok()) << write_record_status;
  EXPECT_TRUE(record_writer.Flush().ok());
  CsvDeltaRecordStreamReader record_reader(string_stream);
  EXPECT_TRUE(record_reader
                  .ReadRecords([&expected](const DataRecord& record) {
                    EXPECT_EQ(*record.UnPack(), expected);
                    return absl::OkStatus();
                  })
                  .ok());
}

TEST(CsvDeltaRecordStreamWriterTest,
     ValidateWritingCsvRecord_KVMutation_StringValue_Base64_Success) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(
      string_stream, CsvDeltaRecordStreamWriter<std::stringstream>::Options{
                         .csv_encoding = CsvEncoding::kBase64});

  DataRecordT expected =
      GetNativeDataRecord(GetKVMutationRecord(GetSimpleStringValue()));

  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  CsvDeltaRecordStreamReader record_reader(
      string_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                         .csv_encoding = CsvEncoding::kBase64});
  EXPECT_TRUE(record_reader
                  .ReadRecords([&expected](const DataRecord& record) {
                    EXPECT_EQ(*record.UnPack(), expected);
                    return absl::OkStatus();
                  })
                  .ok());
}

TEST(CsvDeltaRecordStreamWriterTest,
     ValidateWritingCsvRecord_KVMutation_StringSetValue_Success) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);

  DataRecordT expected =
      GetNativeDataRecord(GetKVMutationRecord(GetStringSetValue({
          "elem1",
          "elem2",
          "elem3",
      })));
  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  CsvDeltaRecordStreamReader record_reader(string_stream);
  EXPECT_TRUE(record_reader
                  .ReadRecords([&expected](const DataRecord& record) {
                    EXPECT_EQ(*record.UnPack(), expected);
                    return absl::OkStatus();
                  })
                  .ok());
}

TEST(CsvDeltaRecordStreamWriterTest,
     ValidateWritingCsvRecord_KVMutation_StringSetValue_Base64_Success) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(
      string_stream, CsvDeltaRecordStreamWriter<std::stringstream>::Options{
                         .csv_encoding = CsvEncoding::kBase64});

  DataRecordT expected =
      GetNativeDataRecord(GetKVMutationRecord(GetStringSetValue({
          "elem1",
          "elem2",
          "elem3",
      })));

  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  CsvDeltaRecordStreamReader record_reader(
      string_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                         .csv_encoding = CsvEncoding::kBase64});
  EXPECT_TRUE(record_reader
                  .ReadRecords([&expected](const DataRecord& record) {
                    EXPECT_EQ(*record.UnPack(), expected);
                    return absl::OkStatus();
                  })
                  .ok());
}

TEST(CsvDeltaRecordStreamWriterTest,
     WritingCsvRecord_KvMutation_UdfConfigHeader_Fails) {
  std::stringstream string_stream;

  CsvDeltaRecordStreamWriter record_writer(
      string_stream,
      CsvDeltaRecordStreamWriter<std::stringstream>::Options{
          .record_type = DataRecordType::kUserDefinedFunctionsConfig});

  DataRecordT expected =
      GetNativeDataRecord(GetKVMutationRecord(GetSimpleStringValue()));
  EXPECT_FALSE(record_writer.WriteRecord(expected).ok());
  record_writer.Close();
}

TEST(CsvDeltaRecordStreamWriterTest,
     ValidateWritingCsvRecord_UdfConfig_Success) {
  std::stringstream string_stream;

  CsvDeltaRecordStreamWriter record_writer(
      string_stream,
      CsvDeltaRecordStreamWriter<std::stringstream>::Options{
          .record_type = DataRecordType::kUserDefinedFunctionsConfig});

  DataRecordT expected = GetNativeDataRecord(GetUserDefinedFunctionsConfig());

  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  CsvDeltaRecordStreamReader record_reader(
      string_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                         .record_type = Record::UserDefinedFunctionsConfig});
  EXPECT_TRUE(record_reader
                  .ReadRecords([&expected](const DataRecord& record) {
                    EXPECT_EQ(*record.UnPack(), expected);
                    return absl::OkStatus();
                  })
                  .ok());
}

TEST(CsvDeltaRecordStreamWriterTest,
     WritingCsvRecord_UdfConfig_KvMutationHeader_Fails) {
  std::stringstream string_stream;

  CsvDeltaRecordStreamWriter record_writer(
      string_stream,
      CsvDeltaRecordStreamWriter<std::stringstream>::Options{
          .record_type = DataRecordType::kKeyValueMutationRecord});

  DataRecordT expected = GetNativeDataRecord(GetUserDefinedFunctionsConfig());

  EXPECT_FALSE(record_writer.WriteRecord(expected).ok());
  record_writer.Close();
}

TEST(CsvDeltaRecordStreamWriterTest,
     ValidateThatWritingUsingClosedWriterFails) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  record_writer.Close();
  EXPECT_FALSE(record_writer.WriteRecord(DataRecordT{}).ok());
}

TEST(CsvDeltaRecordStreamWriterTest, ValidateThatFailedRecordsAreRecoverable) {
  DataRecordT recovered_record;
  CsvDeltaRecordStreamWriter<std::stringstream>::Options options;
  options.fb_struct_recovery_function =
      [&recovered_record](DataRecordT failed_record) {
        recovered_record = failed_record;
      };
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream, options);
  record_writer.Close();

  DataRecordT empty_record;
  EXPECT_EQ(recovered_record, empty_record);

  // Writer is closed, so writing fails.
  DataRecordT expected =
      GetNativeDataRecord(GetKVMutationRecord(GetSimpleStringValue()));

  EXPECT_FALSE(record_writer.WriteRecord(expected).ok());

  EXPECT_EQ(recovered_record, expected);
}

TEST(CsvDeltaRecordStreamWriterTest, ValidateWritingDefaultRecordFails) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  auto status = record_writer.WriteRecord(DataRecordT{});
  EXPECT_FALSE(status.ok()) << status;
}

TEST(CsvDeltaRecordStreamWriterTest, ValidateClosingRecordWriterSucceeds) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  record_writer.Close();
  EXPECT_FALSE(record_writer.IsOpen());
}

TEST(CsvDeltaRecordStreamWriterTest,
     ValidateWritingCsvRecord_ShardMapping_Success) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(
      string_stream, CsvDeltaRecordStreamWriter<std::stringstream>::Options{
                         .record_type = DataRecordType::kShardMappingRecord});

  ShardMappingRecordT shard_mapping_record{.logical_shard = 0,
                                           .physical_shard = 0};
  DataRecordT expected;
  expected.record.Set(std::move(shard_mapping_record));

  auto status = record_writer.WriteRecord(expected);
  EXPECT_TRUE(status.ok()) << status;
  status = record_writer.Flush();
  EXPECT_TRUE(status.ok()) << status;
  CsvDeltaRecordStreamReader record_reader(
      string_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                         .record_type = Record::ShardMappingRecord});
  status = record_reader.ReadRecords([&expected](const DataRecord& record) {
    EXPECT_EQ(*record.UnPack(), expected);
    return absl::OkStatus();
  });
  EXPECT_TRUE(status.ok()) << status;
}

TEST(CsvDeltaRecordStreamWriterTest,
     WritingCsvRecord_ShardMapping_KVMutationHeader_Fails) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(
      string_stream,
      CsvDeltaRecordStreamWriter<std::stringstream>::Options{
          .record_type = DataRecordType::kKeyValueMutationRecord});

  ShardMappingRecordT shard_mapping_record{.logical_shard = 0,
                                           .physical_shard = 0};
  DataRecordT expected;
  expected.record.Set(std::move(shard_mapping_record));

  EXPECT_FALSE(record_writer.WriteRecord(expected).ok());
  record_writer.Close();
}

}  // namespace
}  // namespace kv_server
