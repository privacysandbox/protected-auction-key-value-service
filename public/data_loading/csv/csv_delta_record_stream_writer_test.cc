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

namespace kv_server {
namespace {

KeyValueMutationRecordStruct GetKVMutationRecord(
    KeyValueMutationRecordValueT value = "value") {
  KeyValueMutationRecordStruct record;
  record.key = "key";
  record.value = value;
  record.logical_commit_time = 1234567890;
  record.mutation_type = KeyValueMutationType::Update;
  return record;
}

UserDefinedFunctionsConfigStruct GetUserDefinedFunctionsConfig() {
  UserDefinedFunctionsConfigStruct udf_config_record;
  udf_config_record.language = UserDefinedFunctionsLanguage::Javascript;
  udf_config_record.code_snippet = "function hello(){}";
  udf_config_record.handler_name = "hello";
  udf_config_record.logical_commit_time = 1234567890;
  return udf_config_record;
}

DataRecordStruct GetDataRecord(const RecordT& record) {
  DataRecordStruct data_record;
  data_record.record = record;
  return data_record;
}

TEST(CsvDeltaRecordStreamWriterTest,
     ValidateWritingCsvRecord_KVMutation_StringValue_Success) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);

  DataRecordStruct expected = GetDataRecord(GetKVMutationRecord());
  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  CsvDeltaRecordStreamReader record_reader(string_stream);
  EXPECT_TRUE(record_reader
                  .ReadRecords([&expected](DataRecordStruct record) {
                    EXPECT_EQ(record, expected);
                    return absl::OkStatus();
                  })
                  .ok());
}

TEST(CsvDeltaRecordStreamWriterTest,
     ValidateWritingCsvRecord_KVMutation_SetValue_Success) {
  const std::vector<std::string_view> values{
      "elem1",
      "elem2",
      "elem3",
  };
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);

  DataRecordStruct expected = GetDataRecord(GetKVMutationRecord(values));
  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  CsvDeltaRecordStreamReader record_reader(string_stream);
  EXPECT_TRUE(record_reader
                  .ReadRecords([&expected](DataRecordStruct record) {
                    EXPECT_EQ(record, expected);
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

  DataRecordStruct expected = GetDataRecord(GetKVMutationRecord());
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

  DataRecordStruct expected = GetDataRecord(GetUserDefinedFunctionsConfig());
  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  CsvDeltaRecordStreamReader record_reader(
      string_stream,
      CsvDeltaRecordStreamReader<std::stringstream>::Options{
          .record_type = DataRecordType::kUserDefinedFunctionsConfig});
  EXPECT_TRUE(record_reader
                  .ReadRecords([&expected](DataRecordStruct record) {
                    EXPECT_EQ(record, expected);
                    return absl::OkStatus();
                  })
                  .ok());
}

TEST(CsvDeltaRecordStreamWriterTest,
     WritingCsvRecord_UdfConfig_UdfLanguageUnknown_Fails) {
  std::stringstream string_stream;

  CsvDeltaRecordStreamWriter record_writer(
      string_stream,
      CsvDeltaRecordStreamWriter<std::stringstream>::Options{
          .record_type = DataRecordType::kUserDefinedFunctionsConfig});

  UserDefinedFunctionsConfigStruct udf_config;
  DataRecordStruct data_record = GetDataRecord(udf_config);
  const auto status = record_writer.WriteRecord(data_record);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "Invalid UDF language: ");
  record_writer.Close();
}

TEST(CsvDeltaRecordStreamWriterTest,
     WritingCsvRecord_UdfConfig_KvMutationHeader_Fails) {
  std::stringstream string_stream;

  CsvDeltaRecordStreamWriter record_writer(
      string_stream,
      CsvDeltaRecordStreamWriter<std::stringstream>::Options{
          .record_type = DataRecordType::kKeyValueMutationRecord});

  DataRecordStruct expected = GetDataRecord(GetUserDefinedFunctionsConfig());
  EXPECT_FALSE(record_writer.WriteRecord(expected).ok());
  record_writer.Close();
}

TEST(CsvDeltaRecordStreamWriterTest,
     ValidateThatWritingUsingClosedWriterFails) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  record_writer.Close();
  EXPECT_FALSE(
      record_writer.WriteRecord(GetDataRecord(GetKVMutationRecord())).ok());
}

TEST(CsvDeltaRecordStreamWriterTest, ValidateThatFailedRecordsAreRecoverable) {
  DataRecordStruct recovered_record;
  CsvDeltaRecordStreamWriter<std::stringstream>::Options options;
  options.recovery_function =
      [&recovered_record](DataRecordStruct failed_record) {
        recovered_record = failed_record;
      };
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream, options);
  record_writer.Close();

  DataRecordStruct empty_record;
  EXPECT_EQ(recovered_record, empty_record);

  // Writer is closed, so writing fails.
  DataRecordStruct expected = GetDataRecord(GetKVMutationRecord());
  EXPECT_FALSE(record_writer.WriteRecord(expected).ok());

  EXPECT_EQ(recovered_record, expected);
}

TEST(CsvDeltaRecordStreamWriterTest, ValidateWritingDefaultRecordFails) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  auto status = record_writer.WriteRecord(DataRecordStruct{});
  EXPECT_FALSE(status.ok()) << status;
}

TEST(CsvDeltaRecordStreamWriterTest, ValidateClosingRecordWriterSucceeds) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  record_writer.Close();
  EXPECT_FALSE(record_writer.IsOpen());
}

}  // namespace
}  // namespace kv_server
