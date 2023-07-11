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

#include "tools/data_cli/commands/format_data_command.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/data_loading/csv/csv_delta_record_stream_reader.h"
#include "public/data_loading/csv/csv_delta_record_stream_writer.h"
#include "public/data_loading/readers/delta_record_stream_reader.h"
#include "public/data_loading/writers/delta_record_stream_writer.h"

namespace kv_server {
namespace {

FormatDataCommand::Params GetParams(
    std::string_view record_type = "KEY_VALUE_MUTATION_RECORD") {
  return FormatDataCommand::Params{
      .input_format = "CSV",
      .output_format = "DELTA",
      .csv_column_delimiter = ',',
      .csv_value_delimiter = '|',
      .record_type = std::move(record_type),
  };
}

KeyValueMutationRecordStruct GetKVMutationRecord() {
  KeyValueMutationRecordStruct record;
  record.key = "key";
  record.value = "value";
  record.logical_commit_time = 1234567890;
  record.mutation_type = KeyValueMutationType::Update;
  return record;
}

UserDefinedFunctionsConfigStruct GetUdfConfig() {
  UserDefinedFunctionsConfigStruct udf_config_record;
  udf_config_record.language = UserDefinedFunctionsLanguage::Javascript;
  udf_config_record.code_snippet = "function hello(){}";
  udf_config_record.handler_name = "hello";
  udf_config_record.logical_commit_time = 1234567890;
  udf_config_record.version = 1;
  return udf_config_record;
}

DataRecordStruct GetDataRecord(const RecordT& record) {
  DataRecordStruct data_record;
  data_record.record = record;
  return data_record;
}

KVFileMetadata GetMetadata() {
  KVFileMetadata metadata;
  return metadata;
}

TEST(FormatDataCommandTest, ValidateGeneratingCsvToDeltaData_KVMutations) {
  std::stringstream csv_stream;
  std::stringstream delta_stream;
  CsvDeltaRecordStreamWriter csv_writer(csv_stream);
  EXPECT_TRUE(
      csv_writer.WriteRecord(GetDataRecord(GetKVMutationRecord())).ok());
  EXPECT_TRUE(
      csv_writer.WriteRecord(GetDataRecord(GetKVMutationRecord())).ok());
  EXPECT_TRUE(
      csv_writer.WriteRecord(GetDataRecord(GetKVMutationRecord())).ok());
  csv_writer.Close();
  EXPECT_FALSE(csv_stream.str().empty());
  auto command =
      FormatDataCommand::Create(GetParams(), csv_stream, delta_stream);
  EXPECT_TRUE(command.ok()) << command.status();
  EXPECT_TRUE((*command)->Execute().ok());
  DeltaRecordStreamReader delta_reader(delta_stream);
  testing::MockFunction<absl::Status(DataRecordStruct)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(3)
      .WillRepeatedly([](DataRecordStruct record) {
        EXPECT_EQ(record, GetDataRecord(GetKVMutationRecord()));
        return absl::OkStatus();
      });
  EXPECT_TRUE(delta_reader.ReadRecords(record_callback.AsStdFunction()).ok());
}

TEST(FormatDataCommandTest, ValidateGeneratingDeltaToCsvData_KvMutations) {
  std::stringstream delta_stream;
  std::stringstream csv_stream;
  auto delta_writer = DeltaRecordStreamWriter<std::stringstream>::Create(
      delta_stream, DeltaRecordWriter::Options{.metadata = GetMetadata()});
  EXPECT_TRUE(delta_writer.ok()) << delta_writer.status();
  EXPECT_TRUE(
      (*delta_writer)->WriteRecord(GetDataRecord(GetKVMutationRecord())).ok());
  EXPECT_TRUE(
      (*delta_writer)->WriteRecord(GetDataRecord(GetKVMutationRecord())).ok());
  EXPECT_TRUE(
      (*delta_writer)->WriteRecord(GetDataRecord(GetKVMutationRecord())).ok());
  EXPECT_TRUE(
      (*delta_writer)->WriteRecord(GetDataRecord(GetKVMutationRecord())).ok());
  EXPECT_TRUE(
      (*delta_writer)->WriteRecord(GetDataRecord(GetKVMutationRecord())).ok());
  (*delta_writer)->Close();
  auto command = FormatDataCommand::Create(
      FormatDataCommand::Params{
          .input_format = "DELTA",
          .output_format = "CSV",
          .csv_column_delimiter = ',',
          .csv_value_delimiter = '|',
          .record_type = "KEY_VALUE_MUTATION_RECORD",
      },
      delta_stream, csv_stream);
  EXPECT_TRUE(command.ok()) << command.status();
  EXPECT_TRUE((*command)->Execute().ok());
  CsvDeltaRecordStreamReader csv_reader(csv_stream);
  testing::MockFunction<absl::Status(DataRecordStruct)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(5)
      .WillRepeatedly([](DataRecordStruct record) {
        EXPECT_EQ(record, GetDataRecord(GetKVMutationRecord()));
        return absl::OkStatus();
      });
  EXPECT_TRUE(csv_reader.ReadRecords(record_callback.AsStdFunction()).ok());
}

TEST(FormatDataCommandTest, ValidateGeneratingCsvToDeltaData_UdfConfig) {
  std::stringstream csv_stream;
  std::stringstream delta_stream;
  CsvDeltaRecordStreamWriter csv_writer(
      csv_stream,
      CsvDeltaRecordStreamWriter<std::stringstream>::Options{
          .record_type = DataRecordType::kUserDefinedFunctionsConfig});
  EXPECT_TRUE(csv_writer.WriteRecord(GetDataRecord(GetUdfConfig())).ok());
  EXPECT_TRUE(csv_writer.WriteRecord(GetDataRecord(GetUdfConfig())).ok());
  EXPECT_TRUE(csv_writer.WriteRecord(GetDataRecord(GetUdfConfig())).ok());
  csv_writer.Close();
  EXPECT_FALSE(csv_stream.str().empty());
  auto command = FormatDataCommand::Create(
      GetParams(/*record_type=*/"USER_DEFINED_FUNCTIONS_CONFIG"), csv_stream,
      delta_stream);
  EXPECT_TRUE(command.ok()) << command.status();
  EXPECT_TRUE((*command)->Execute().ok());
  DeltaRecordStreamReader delta_reader(delta_stream);
  testing::MockFunction<absl::Status(DataRecordStruct)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(3)
      .WillRepeatedly([](DataRecordStruct record) {
        EXPECT_EQ(record, GetDataRecord(GetUdfConfig()));
        return absl::OkStatus();
      });
  EXPECT_TRUE(delta_reader.ReadRecords(record_callback.AsStdFunction()).ok());
}

TEST(FormatDataCommandTest, ValidateGeneratingDeltaToCsvData_UdfConfig) {
  std::stringstream delta_stream;
  std::stringstream csv_stream;
  auto delta_writer = DeltaRecordStreamWriter<std::stringstream>::Create(
      delta_stream, DeltaRecordWriter::Options{.metadata = GetMetadata()});
  EXPECT_TRUE(delta_writer.ok()) << delta_writer.status();
  EXPECT_TRUE((*delta_writer)->WriteRecord(GetDataRecord(GetUdfConfig())).ok());
  EXPECT_TRUE((*delta_writer)->WriteRecord(GetDataRecord(GetUdfConfig())).ok());
  EXPECT_TRUE((*delta_writer)->WriteRecord(GetDataRecord(GetUdfConfig())).ok());
  (*delta_writer)->Close();
  auto command = FormatDataCommand::Create(
      FormatDataCommand::Params{
          .input_format = "DELTA",
          .output_format = "CSV",
          .csv_column_delimiter = ',',
          .csv_value_delimiter = '|',
          .record_type = "USER_DEFINED_FUNCTIONS_CONFIG",
      },
      delta_stream, csv_stream);
  EXPECT_TRUE(command.ok()) << command.status();
  EXPECT_TRUE((*command)->Execute().ok());
  CsvDeltaRecordStreamReader csv_reader(
      csv_stream,
      CsvDeltaRecordStreamReader<std::stringstream>::Options{
          .record_type = DataRecordType::kUserDefinedFunctionsConfig,
      });
  testing::MockFunction<absl::Status(DataRecordStruct)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(3)
      .WillRepeatedly([](DataRecordStruct record) {
        EXPECT_EQ(record, GetDataRecord(GetUdfConfig()));
        return absl::OkStatus();
      });
  EXPECT_TRUE(csv_reader.ReadRecords(record_callback.AsStdFunction()).ok());
}

TEST(FormatDataCommandTest, ValidateIncorrectInputParams) {
  std::stringstream unused_stream;
  auto params = GetParams();
  params.input_format = "";
  absl::Status status =
      FormatDataCommand::Create(params, unused_stream, unused_stream).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_STREQ(status.message().data(), "Input format cannot be empty.")
      << status;
  params.input_format = "UNSUPPORTED_FORMAT";
  status =
      FormatDataCommand::Create(params, unused_stream, unused_stream).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_STREQ(status.message().data(),
               "Input format: UNSUPPORTED_FORMAT is not supported.")
      << status;
}

TEST(FormatDataCommandTest, ValidateIncorrectRecordTypeParams) {
  std::stringstream unused_stream;
  auto params = GetParams("");
  absl::Status status =
      FormatDataCommand::Create(params, unused_stream, unused_stream).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_STREQ(status.message().data(), "Record type cannot be empty.")
      << status;
  params.record_type = "invalid record type";
  status =
      FormatDataCommand::Create(params, unused_stream, unused_stream).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_STREQ(status.message().data(),
               "Record type invalid record type is not supported.")
      << status;
}

TEST(FormatDataCommandTest, ValidateIncorrectOutputParams) {
  std::stringstream unused_stream;
  auto params = GetParams();
  params.output_format = "";
  absl::Status status =
      FormatDataCommand::Create(params, unused_stream, unused_stream).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_STREQ(status.message().data(), "Output format cannot be empty.")
      << status;
  params.output_format = "delta";
  status =
      FormatDataCommand::Create(params, unused_stream, unused_stream).status();
  EXPECT_TRUE(status.ok());
  params.output_format = "UNSUPPORTED_FORMAT";
  status =
      FormatDataCommand::Create(params, unused_stream, unused_stream).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_STREQ(status.message().data(),
               "Output format: UNSUPPORTED_FORMAT is not supported.")
      << status;
}

}  // namespace
}  // namespace kv_server
