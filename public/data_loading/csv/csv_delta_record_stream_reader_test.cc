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

#include "public/data_loading/csv/csv_delta_record_stream_reader.h"

#include <sstream>

#include "absl/log/log.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/data_loading/csv/csv_delta_record_stream_writer.h"
#include "public/data_loading/record_utils.h"
#include "public/test_util/data_record.h"

namespace kv_server {
namespace {

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingAndWriting_KVMutation_StringValues_Success) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  const auto expected =
      GetNativeDataRecord(GetKVMutationRecord(GetSimpleStringValue()));
  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  LOG(INFO) << string_stream.str();
  CsvDeltaRecordStreamReader record_reader(string_stream);
  auto status =
      record_reader.ReadRecords([&expected](const DataRecord& record) {
        EXPECT_EQ(*record.UnPack(), expected);
        return absl::OkStatus();
      });
  EXPECT_TRUE(status.ok()) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingAndWriting_KVMutation_StringValues_Base64_Success) {
  std::stringstream string_stream;

  CsvDeltaRecordStreamWriter record_writer(
      string_stream, CsvDeltaRecordStreamWriter<std::stringstream>::Options{
                         .csv_encoding = CsvEncoding::kBase64});
  const auto expected =
      GetNativeDataRecord(GetKVMutationRecord(GetSimpleStringValue()));
  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  LOG(INFO) << string_stream.str();
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

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingAndWriting_KVMutation_StringValues_Base64_Invalid_Failure) {
  const char data[] =
      R"csv(key,value,value_type,mutation_type,logical_commit_time
  key,value,string,Update,1)csv";
  std::stringstream csv_stream;
  csv_stream.str(data);
  CsvDeltaRecordStreamReader record_reader(
      csv_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                      .csv_encoding = CsvEncoding::kBase64});
  auto status = record_reader.ReadRecords(
      [](const DataRecord& record) { return absl::OkStatus(); });
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.message(), "base64 decode failed for value: value")
      << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingCsvRecords_KVMutation_InvalidTimestamps_Failure) {
  const char invalid_data[] =
      R"csv(key,value,value_type,mutation_type,logical_commit_time
  key,value,string,Update,invalid_time)csv";
  std::stringstream csv_stream;
  csv_stream.str(invalid_data);
  CsvDeltaRecordStreamReader record_reader(csv_stream);
  absl::Status status = record_reader.ReadRecords(
      [](const DataRecord&) { return absl::OkStatus(); });
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.message(),
            "Cannot convert logical_commit_time:invalid_time to a number.")
      << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingCsvRecords_KVMutation_InvalidMutation_Failure) {
  const char invalid_data[] =
      R"csv(key,value,value_type,mutation_type,logical_commit_time
  key,value,string,invalid_mutation,1000000)csv";
  std::stringstream csv_stream;
  csv_stream.str(invalid_data);
  CsvDeltaRecordStreamReader record_reader(csv_stream);
  absl::Status status = record_reader.ReadRecords(
      [](const DataRecord&) { return absl::OkStatus(); });
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.message(), "Unknown mutation type:invalid_mutation")
      << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingAndWriting_KVMutation_SetValues_Success) {
  const std::vector<std::string_view> values{
      "elem1",
      "elem2",
      "elem3",
  };

  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);

  const auto expected =
      GetNativeDataRecord(GetKVMutationRecord(GetStringSetValue(values)));
  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  LOG(INFO) << string_stream.str();
  CsvDeltaRecordStreamReader record_reader(string_stream);
  auto status =
      record_reader.ReadRecords([&expected](const DataRecord& record) {
        EXPECT_EQ(*record.UnPack(), expected);
        return absl::OkStatus();
      });
  EXPECT_TRUE(status.ok()) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingAndWriting_KVMutation_UInt32SetValues_Success) {
  const std::vector<uint32_t> values{
      1000,
      1001,
      1002,
  };
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  const auto expected = GetNativeDataRecord(
      GetKVMutationRecord(GetUIntSetValue<uint32_t, UInt32SetT>(values)));
  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  LOG(INFO) << string_stream.str();
  CsvDeltaRecordStreamReader record_reader(string_stream);
  auto status =
      record_reader.ReadRecords([&expected](const DataRecord& record) {
        EXPECT_EQ(*record.UnPack(), expected);
        return absl::OkStatus();
      });
  EXPECT_TRUE(status.ok()) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingAndWriting_KVMutation_UInt64SetValues_Success) {
  const std::vector<uint64_t> values{
      18446744073709551613UL,
      18446744073709551614UL,
      18446744073709551615UL,
  };
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  const auto expected = GetNativeDataRecord(
      GetKVMutationRecord(GetUIntSetValue<uint64_t, UInt64SetT>(values)));
  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  LOG(INFO) << string_stream.str();
  CsvDeltaRecordStreamReader record_reader(string_stream);
  auto status =
      record_reader.ReadRecords([&expected](const DataRecord& record) {
        EXPECT_EQ(*record.UnPack(), expected);
        return absl::OkStatus();
      });
  EXPECT_TRUE(status.ok()) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingAndWriting_KVMutation_SetValues_Base64_Success) {
  const std::vector<std::string_view> values{
      "elem1",
      "elem2",
      "elem3",
  };
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(
      string_stream, CsvDeltaRecordStreamWriter<std::stringstream>::Options{
                         .csv_encoding = CsvEncoding::kBase64});

  const auto expected =
      GetNativeDataRecord(GetKVMutationRecord(GetStringSetValue(values)));
  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  LOG(INFO) << string_stream.str();
  CsvDeltaRecordStreamReader record_reader(
      string_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                         .csv_encoding = CsvEncoding::kBase64});
  auto status =
      record_reader.ReadRecords([&expected](const DataRecord& record) {
        EXPECT_EQ(*record.UnPack(), expected);
        return absl::OkStatus();
      });
  EXPECT_TRUE(status.ok()) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingAndWriting_KVMutation_SetValues_Base64_Invalid_Failure) {
  const char data[] =
      R"csv(key,value,value_type,mutation_type,logical_commit_time
  key,value,string,Update,1)csv";
  std::stringstream csv_stream;
  csv_stream.str(data);
  CsvDeltaRecordStreamReader record_reader(
      csv_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                      .csv_encoding = CsvEncoding::kBase64});
  const auto status = record_reader.ReadRecords(
      [](const DataRecord&) { return absl::OkStatus(); });
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ReadingCsvRecords_KvMutation_UdfConfigHeader_Failure) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  const auto input =
      GetNativeDataRecord(GetKVMutationRecord(GetSimpleStringValue()));
  EXPECT_TRUE(record_writer.WriteRecord(input).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  CsvDeltaRecordStreamReader record_reader(
      string_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                         .record_type = Record::UserDefinedFunctionsConfig});
  const auto status = record_reader.ReadRecords(
      [](const DataRecord&) { return absl::OkStatus(); });
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingAndWriting_UdfConfig_Success) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(
      string_stream,
      CsvDeltaRecordStreamWriter<std::stringstream>::Options{
          .record_type = DataRecordType::kUserDefinedFunctionsConfig});
  const auto expected = GetNativeDataRecord(GetUserDefinedFunctionsConfig());
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

TEST(CsvDeltaRecordStreamReaderTest,
     ReadingAndWriting_UdfConfig_KvMutationHeader_Failure) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(
      string_stream,
      CsvDeltaRecordStreamWriter<std::stringstream>::Options{
          .record_type = DataRecordType::kUserDefinedFunctionsConfig});
  const auto expected = GetNativeDataRecord(GetUserDefinedFunctionsConfig());
  EXPECT_TRUE(record_writer.WriteRecord(expected).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  CsvDeltaRecordStreamReader record_reader(
      string_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                         .record_type = Record::KeyValueMutationRecord});
  const auto status = record_reader.ReadRecords(
      [](const DataRecord&) { return absl::OkStatus(); });
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingCsvRecords_UdfConfig_InvalidTimestamps_Failure) {
  const char invalid_data[] =
      R"csv(code_snippet,handler_name,logical_commit_time,language,version
  function hello(){},hello,invalid_time,javascript,1)csv";
  std::stringstream csv_stream;
  csv_stream.str(invalid_data);
  CsvDeltaRecordStreamReader record_reader(
      csv_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                      .record_type = Record::UserDefinedFunctionsConfig});
  absl::Status status = record_reader.ReadRecords(
      [](const DataRecord&) { return absl::OkStatus(); });
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.message(),
            "Cannot convert logical_commit_time:invalid_time to a number.")
      << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingCsvRecords_UdfConfig_InvalidVersion_Failure) {
  const char invalid_data[] =
      R"csv(code_snippet,handler_name,logical_commit_time,language,version
  function hello(){},hello,1,javascript,invalid_version)csv";
  std::stringstream csv_stream;
  csv_stream.str(invalid_data);
  CsvDeltaRecordStreamReader record_reader(
      csv_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                      .record_type = Record::UserDefinedFunctionsConfig});
  absl::Status status = record_reader.ReadRecords(
      [](const DataRecord&) { return absl::OkStatus(); });
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.message(),
            "Cannot convert version:invalid_version to a number.")
      << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingCsvRecords_UdfConfig_InvalidLanguage_Failure) {
  const char invalid_data[] =
      R"csv(code_snippet,handler_name,logical_commit_time,language,version
  function hello(){},hello,1000000,invalid_language,1)csv";
  std::stringstream csv_stream;
  csv_stream.str(invalid_data);
  CsvDeltaRecordStreamReader record_reader(
      csv_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                      .record_type = Record::UserDefinedFunctionsConfig});
  absl::Status status = record_reader.ReadRecords(
      [](const DataRecord&) { return absl::OkStatus(); });
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.message(), "Language: invalid_language is not supported.")
      << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingAndWriting_ShardMapping_Success) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(
      string_stream, CsvDeltaRecordStreamWriter<std::stringstream>::Options{
                         .record_type = DataRecordType::kShardMappingRecord});
  const auto expected = GetNativeDataRecord(
      ShardMappingRecordT{.logical_shard = 0, .physical_shard = 0});
  auto status = record_writer.WriteRecord(expected);
  EXPECT_TRUE(status.ok()) << status;
  status = record_writer.Flush();
  EXPECT_TRUE(status.ok());
  CsvDeltaRecordStreamReader record_reader(
      string_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                         .record_type = Record::ShardMappingRecord});
  status = record_reader.ReadRecords([&expected](const DataRecord& record) {
    EXPECT_EQ(*record.UnPack(), expected);
    return absl::OkStatus();
  });
  EXPECT_TRUE(status.ok()) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingCsvRecords_ShardMapping_InvalidNumericColumn_Failure) {
  const char invalid_data[] =
      R"csv(logical_shard,physical_shard
  not_a_number,1)csv";
  std::stringstream csv_stream;
  csv_stream.str(invalid_data);
  CsvDeltaRecordStreamReader record_reader(
      csv_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                      .record_type = Record::ShardMappingRecord});
  absl::Status status = record_reader.ReadRecords(
      [](const DataRecord&) { return absl::OkStatus(); });
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.message(),
            "Cannot convert logical_shard:  not_a_number to a number.")
      << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
}

}  // namespace
}  // namespace kv_server
