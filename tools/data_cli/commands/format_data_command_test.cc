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

#include <vector>

#include "absl/strings/escaping.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/data_loading/csv/csv_delta_record_stream_reader.h"
#include "public/data_loading/csv/csv_delta_record_stream_writer.h"
#include "public/data_loading/readers/delta_record_stream_reader.h"
#include "public/data_loading/writers/delta_record_stream_writer.h"
#include "public/test_util/data_record.h"

namespace kv_server {
namespace {

FormatDataCommand::Params GetParams(
    std::string_view record_type = "KEY_VALUE_MUTATION_RECORD") {
  return FormatDataCommand::Params{
      .input_format = "CSV",
      .output_format = "DELTA",
      .csv_column_delimiter = ',',
      .csv_value_delimiter = '|',
      .record_type = std::string(record_type),
  };
}

KeyValueMutationRecordStruct GetKVMutationRecord(
    KeyValueMutationRecordValueT value = "value") {
  KeyValueMutationRecordStruct record;
  record.key = "key";
  record.value = value;
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

class FormatDataCommandTest
    : public testing::TestWithParam<KeyValueMutationRecordValueT> {
 protected:
  KeyValueMutationRecordValueT GetValue() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(KVMutationValue, FormatDataCommandTest,
                         testing::Values("value",
                                         std::vector<std::string_view>{
                                             "value1", "value2", "value3"}));

TEST_P(FormatDataCommandTest, ValidateGeneratingCsvToDeltaData_KVMutations) {
  std::stringstream csv_stream;
  std::stringstream delta_stream;
  CsvDeltaRecordStreamWriter csv_writer(csv_stream);
  const auto& record =
      GetNativeDataRecord(GetKVMutationRecord(GetSimpleStringValue()));
  EXPECT_TRUE(csv_writer.WriteRecord(record).ok());
  EXPECT_TRUE(csv_writer.WriteRecord(record).ok());
  EXPECT_TRUE(csv_writer.WriteRecord(record).ok());
  csv_writer.Close();
  EXPECT_FALSE(csv_stream.str().empty());
  auto command =
      FormatDataCommand::Create(GetParams(), csv_stream, delta_stream);
  EXPECT_TRUE(command.ok()) << command.status();
  EXPECT_TRUE((*command)->Execute().ok());
  DeltaRecordStreamReader delta_reader(delta_stream);
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(3)
      .WillRepeatedly([&record](const DataRecord& actual_record) {
        EXPECT_EQ(*actual_record.UnPack(), record);
        return absl::OkStatus();
      });
  EXPECT_TRUE(delta_reader.ReadRecords(record_callback.AsStdFunction()).ok());
}

TEST_P(FormatDataCommandTest, ValidateGeneratingDeltaToCsvData_KvMutations) {
  std::stringstream delta_stream;
  std::stringstream csv_stream;
  auto delta_writer = DeltaRecordStreamWriter<std::stringstream>::Create(
      delta_stream, DeltaRecordWriter::Options{.metadata = GetMetadata()});
  const auto& record = GetDataRecord(GetKVMutationRecord(GetValue()));
  EXPECT_TRUE(delta_writer.ok()) << delta_writer.status();
  EXPECT_TRUE((*delta_writer)->WriteRecord(record).ok());
  EXPECT_TRUE((*delta_writer)->WriteRecord(record).ok());
  EXPECT_TRUE((*delta_writer)->WriteRecord(record).ok());
  EXPECT_TRUE((*delta_writer)->WriteRecord(record).ok());
  EXPECT_TRUE((*delta_writer)->WriteRecord(record).ok());
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
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(5)
      .WillRepeatedly([&record](const DataRecord& actual_record) {
        std::unique_ptr<DataRecordT> data_record_native(actual_record.UnPack());
        auto [fbs_buffer, serialized_string_view] =
            Serialize(*data_record_native);
        EXPECT_TRUE(DeserializeDataRecord(
                        serialized_string_view,
                        [&record](const DataRecordStruct& data_record) {
                          EXPECT_EQ(data_record, record);
                          return absl::OkStatus();
                        })
                        .ok());

        return absl::OkStatus();
      });
  EXPECT_TRUE(csv_reader.ReadRecords(record_callback.AsStdFunction()).ok());
}

TEST(FormatDataCommandTest,
     ValidateGeneratingCsvToDeltaData_KVMutations_Base64) {
  std::stringstream csv_stream;
  std::stringstream delta_stream;
  CsvDeltaRecordStreamWriter csv_writer(csv_stream);

  std::string plaintext_value = "value";
  std::string base64_value;
  absl::Base64Escape(plaintext_value, &base64_value);
  const auto& base64_record = GetNativeDataRecord(
      GetKVMutationRecord(GetSimpleStringValue(base64_value)));
  EXPECT_TRUE(csv_writer.WriteRecord(base64_record).ok());
  EXPECT_TRUE(csv_writer.WriteRecord(base64_record).ok());
  EXPECT_TRUE(csv_writer.WriteRecord(base64_record).ok());
  csv_writer.Close();
  EXPECT_FALSE(csv_stream.str().empty());
  auto params = GetParams();
  params.csv_encoding = "BASE64";
  auto command = FormatDataCommand::Create(params, csv_stream, delta_stream);
  EXPECT_TRUE(command.ok()) << command.status();
  EXPECT_TRUE((*command)->Execute().ok());
  DeltaRecordStreamReader delta_reader(delta_stream);
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  const auto expected_record = GetNativeDataRecord(
      GetKVMutationRecord(GetSimpleStringValue(plaintext_value)));
  EXPECT_CALL(record_callback, Call)
      .Times(3)
      .WillRepeatedly([&expected_record](const DataRecord& actual_record) {
        EXPECT_EQ(*actual_record.UnPack(), expected_record);
        return absl::OkStatus();
      });
  EXPECT_TRUE(delta_reader.ReadRecords(record_callback.AsStdFunction()).ok());
}

TEST(FormatDataCommandTest,
     ValidateGeneratingDeltaToCsvData_KvMutations_Base64) {
  std::stringstream delta_stream;
  std::stringstream csv_stream;
  auto delta_writer = DeltaRecordStreamWriter<std::stringstream>::Create(
      delta_stream, DeltaRecordWriter::Options{.metadata = GetMetadata()});
  std::string plaintext_value = "value";
  const auto& plaintext_record =
      GetDataRecord(GetKVMutationRecord(plaintext_value));
  EXPECT_TRUE(delta_writer.ok()) << delta_writer.status();
  EXPECT_TRUE((*delta_writer)->WriteRecord(plaintext_record).ok());
  EXPECT_TRUE((*delta_writer)->WriteRecord(plaintext_record).ok());
  EXPECT_TRUE((*delta_writer)->WriteRecord(plaintext_record).ok());
  EXPECT_TRUE((*delta_writer)->WriteRecord(plaintext_record).ok());
  EXPECT_TRUE((*delta_writer)->WriteRecord(plaintext_record).ok());
  (*delta_writer)->Close();
  auto command = FormatDataCommand::Create(
      FormatDataCommand::Params{
          .input_format = "DELTA",
          .output_format = "CSV",
          .csv_column_delimiter = ',',
          .csv_value_delimiter = '|',
          .record_type = "KEY_VALUE_MUTATION_RECORD",
          .csv_encoding = "BASE64",
      },
      delta_stream, csv_stream);
  EXPECT_TRUE(command.ok()) << command.status();
  EXPECT_TRUE((*command)->Execute().ok());
  CsvDeltaRecordStreamReader csv_reader(csv_stream);
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;

  std::string base64_value;
  absl::Base64Escape(plaintext_value, &base64_value);
  const auto& expected_record =
      GetDataRecord(GetKVMutationRecord(base64_value));
  EXPECT_CALL(record_callback, Call)
      .Times(5)
      .WillRepeatedly([&expected_record](const DataRecord& actual_record) {
        std::unique_ptr<DataRecordT> data_record_native(actual_record.UnPack());
        auto [fbs_buffer, serialized_string_view] =
            Serialize(*data_record_native);
        EXPECT_TRUE(
            DeserializeDataRecord(
                serialized_string_view,
                [&expected_record](const DataRecordStruct& data_record) {
                  EXPECT_EQ(data_record, expected_record);
                  return absl::OkStatus();
                })
                .ok());

        return absl::OkStatus();
      });
  EXPECT_TRUE(csv_reader.ReadRecords(record_callback.AsStdFunction()).ok());
}

TEST(FormatDataCommandTest,
     ValidateGeneratingCsvToDeltaData_KVMutations_InvalidEncoding) {
  std::stringstream csv_stream;
  std::stringstream delta_stream;
  CsvDeltaRecordStreamWriter csv_writer(csv_stream);

  std::string plaintext_value;
  std::string base64_value;
  absl::Base64Escape(plaintext_value, &base64_value);
  const auto& record = GetNativeDataRecord(
      GetKVMutationRecord(GetSimpleStringValue(base64_value)));
  EXPECT_TRUE(csv_writer.WriteRecord(record).ok());
  EXPECT_TRUE(csv_writer.WriteRecord(record).ok());
  EXPECT_TRUE(csv_writer.WriteRecord(record).ok());
  csv_writer.Close();
  EXPECT_FALSE(csv_stream.str().empty());
  auto params = GetParams();
  params.csv_encoding = "UNKNOWN";
  auto command = FormatDataCommand::Create(params, csv_stream, delta_stream);
  EXPECT_FALSE(command.ok()) << command.status();
}

TEST(FormatDataCommandTest, ValidateGeneratingCsvToDeltaData_UdfConfig) {
  std::stringstream csv_stream;
  std::stringstream delta_stream;
  CsvDeltaRecordStreamWriter csv_writer(
      csv_stream,
      CsvDeltaRecordStreamWriter<std::stringstream>::Options{
          .record_type = DataRecordType::kUserDefinedFunctionsConfig});
  auto udf_config_record = GetNativeDataRecord(GetUserDefinedFunctionsConfig());
  EXPECT_TRUE(csv_writer.WriteRecord(udf_config_record).ok());
  EXPECT_TRUE(csv_writer.WriteRecord(udf_config_record).ok());
  EXPECT_TRUE(csv_writer.WriteRecord(udf_config_record).ok());
  csv_writer.Close();
  EXPECT_FALSE(csv_stream.str().empty());
  auto command = FormatDataCommand::Create(
      GetParams(/*record_type=*/"USER_DEFINED_FUNCTIONS_CONFIG"), csv_stream,
      delta_stream);
  EXPECT_TRUE(command.ok()) << command.status();
  EXPECT_TRUE((*command)->Execute().ok());
  DeltaRecordStreamReader delta_reader(delta_stream);
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(3)
      .WillRepeatedly([&udf_config_record](const DataRecord& actual_record) {
        EXPECT_EQ(*actual_record.UnPack(), udf_config_record);
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
      csv_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                      .record_type = Record::UserDefinedFunctionsConfig,
                  });
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(3)
      .WillRepeatedly([](const DataRecord& actual_record) {
        std::unique_ptr<DataRecordT> data_record_native(actual_record.UnPack());
        auto [fbs_buffer, serialized_string_view] =
            Serialize(*data_record_native);
        EXPECT_TRUE(DeserializeDataRecord(
                        serialized_string_view,
                        [](const DataRecordStruct& data_record) {
                          EXPECT_EQ(data_record, GetDataRecord(GetUdfConfig()));
                          return absl::OkStatus();
                        })
                        .ok());

        return absl::OkStatus();
      });
  EXPECT_TRUE(csv_reader.ReadRecords(record_callback.AsStdFunction()).ok());
}

TEST(FormatDataCommandTest,
     ValidateGeneratingDeltaToCsvData_ShardMappingRecord) {
  std::stringstream delta_stream;
  std::stringstream csv_stream;
  auto delta_writer = DeltaRecordStreamWriter<std::stringstream>::Create(
      delta_stream, DeltaRecordWriter::Options{.metadata = GetMetadata()});
  EXPECT_TRUE(delta_writer.ok()) << delta_writer.status();
  EXPECT_TRUE((*delta_writer)
                  ->WriteRecord(GetDataRecord(ShardMappingRecordStruct{
                      .logical_shard = 0,
                      .physical_shard = 0,
                  }))
                  .ok());
  EXPECT_TRUE((*delta_writer)
                  ->WriteRecord(GetDataRecord(ShardMappingRecordStruct{
                      .logical_shard = 0,
                      .physical_shard = 0,
                  }))
                  .ok());
  EXPECT_TRUE((*delta_writer)
                  ->WriteRecord(GetDataRecord(ShardMappingRecordStruct{
                      .logical_shard = 0,
                      .physical_shard = 0,
                  }))
                  .ok());
  (*delta_writer)->Close();
  auto command = FormatDataCommand::Create(
      FormatDataCommand::Params{
          .input_format = "DELTA",
          .output_format = "CSV",
          .csv_column_delimiter = ',',
          .csv_value_delimiter = '|',
          .record_type = "SHARD_MAPPING_RECORD",
      },
      delta_stream, csv_stream);
  EXPECT_TRUE(command.ok()) << command.status();
  auto status = (*command)->Execute();
  EXPECT_TRUE(status.ok()) << status;
  CsvDeltaRecordStreamReader csv_reader(
      csv_stream, CsvDeltaRecordStreamReader<std::stringstream>::Options{
                      .record_type = Record::ShardMappingRecord,
                  });
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(3)
      .WillRepeatedly([](const DataRecord& actual_record) {
        std::unique_ptr<DataRecordT> data_record_native(actual_record.UnPack());
        auto [fbs_buffer, serialized_string_view] =
            Serialize(*data_record_native);
        EXPECT_TRUE(DeserializeDataRecord(
                        serialized_string_view,
                        [](const DataRecordStruct& data_record) {
                          EXPECT_EQ(data_record,
                                    GetDataRecord(ShardMappingRecordStruct{
                                        .logical_shard = 0,
                                        .physical_shard = 0,
                                    }));
                          return absl::OkStatus();
                        })
                        .ok());

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
  EXPECT_EQ(status.message(), "Input format cannot be empty.") << status;
  params.input_format = "UNSUPPORTED_FORMAT";
  status =
      FormatDataCommand::Create(params, unused_stream, unused_stream).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_EQ(status.message(),
            "Input format: UNSUPPORTED_FORMAT is not supported.")
      << status;
}

TEST(FormatDataCommandTest, ValidateIncorrectRecordTypeParams) {
  std::stringstream unused_stream;
  auto params = GetParams("");
  absl::Status status =
      FormatDataCommand::Create(params, unused_stream, unused_stream).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_EQ(status.message(), "Record type cannot be empty.") << status;
  params.record_type = "invalid record type";
  status =
      FormatDataCommand::Create(params, unused_stream, unused_stream).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_EQ(status.message(),
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
  EXPECT_EQ(status.message(), "Output format cannot be empty.") << status;
  params.output_format = "delta";
  status =
      FormatDataCommand::Create(params, unused_stream, unused_stream).status();
  EXPECT_TRUE(status.ok());
  params.output_format = "UNSUPPORTED_FORMAT";
  status =
      FormatDataCommand::Create(params, unused_stream, unused_stream).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_EQ(status.message(),
            "Output format: UNSUPPORTED_FORMAT is not supported.")
      << status;
}

TEST(FormatDataCommandTest,
     ValidateGeneratingCsvToDeltaData_KVMutations_ShardNum) {
  std::stringstream csv_stream;
  std::stringstream delta_stream;
  CsvDeltaRecordStreamWriter csv_writer(csv_stream);
  const auto& record =
      GetNativeDataRecord(GetKVMutationRecord(GetSimpleStringValue()));
  EXPECT_TRUE(csv_writer.WriteRecord(record).ok());
  EXPECT_TRUE(csv_writer.WriteRecord(record).ok());
  EXPECT_TRUE(csv_writer.WriteRecord(record).ok());
  csv_writer.Close();
  EXPECT_FALSE(csv_stream.str().empty());
  auto params = GetParams();
  params.shard_number = 2;
  params.number_of_shards = 3;
  auto command = FormatDataCommand::Create(params, csv_stream, delta_stream);
  EXPECT_TRUE(command.ok()) << command.status();
  EXPECT_TRUE((*command)->Execute().ok());
  DeltaRecordStreamReader delta_reader(delta_stream);
  auto metadata = delta_reader.ReadMetadata();
  EXPECT_TRUE(metadata.ok());
  EXPECT_EQ(metadata->sharding_metadata().shard_num(), 2);
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(3)
      .WillRepeatedly([&record](const DataRecord& actual_record) {
        EXPECT_EQ(*actual_record.UnPack(), record);
        return absl::OkStatus();
      });
  EXPECT_TRUE(delta_reader.ReadRecords(record_callback.AsStdFunction()).ok());
}

TEST(FormatDataCommandTest, ValidateIncorrectShardingMetadataParams) {
  std::stringstream unused_stream;
  auto params = GetParams();
  params.shard_number = 2;
  absl::Status status =
      FormatDataCommand::Create(params, unused_stream, unused_stream).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_EQ(status.message(),
            "Shard metadata is invalid. shard_number is 2 and "
            "number_of_shards is -1. Valid inputs must satisfy the "
            "requirement: 0 <= shard_number < number_of_shards")
      << status;
}

}  // namespace
}  // namespace kv_server
