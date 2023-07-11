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

#include "public/data_loading/writers/delta_record_stream_writer.h"

#include <memory>
#include <sstream>

#include "gtest/gtest.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/readers/riegeli_stream_io.h"
#include "public/data_loading/records_utils.h"

namespace kv_server {
namespace {

KVFileMetadata GetMetadata() {
  KVFileMetadata metadata;
  return metadata;
}

KeyValueMutationRecordStruct GetKeyValueMutationRecord() {
  KeyValueMutationRecordStruct kv_mutation_record;
  kv_mutation_record.key = "key";
  kv_mutation_record.value = "value";
  kv_mutation_record.logical_commit_time = 1234567890;
  kv_mutation_record.mutation_type = KeyValueMutationType::Update;
  return kv_mutation_record;
}

UserDefinedFunctionsConfigStruct GetUserDefinedFunctionsConfig() {
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

KeyValueMutationRecordStruct GetDeltaSetRecord() {
  KeyValueMutationRecordStruct record;
  record.key = "key";
  record.value = std::vector<std::string_view>{"v1", "v2"};
  record.logical_commit_time = 1234567890;
  record.mutation_type = KeyValueMutationType::Update;
  return record;
}

class DeltaRecordStreamWriterTest
    : public testing::TestWithParam<DeltaRecordWriter::Options> {
 protected:
  template <typename StreamT>
  absl::StatusOr<std::unique_ptr<DeltaRecordStreamWriter<StreamT>>>
  CreateDeltaRecordStreamWriter(StreamT& stream) {
    return DeltaRecordStreamWriter<StreamT>::Create(
        stream, (DeltaRecordWriter::Options&)GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    ValidMetadata, DeltaRecordStreamWriterTest,
    testing::Values(DeltaRecordWriter::Options{.enable_compression = false,
                                               .metadata = GetMetadata()},
                    DeltaRecordWriter::Options{.enable_compression = true,
                                               .metadata = GetMetadata()}));

TEST_P(DeltaRecordStreamWriterTest,
       ValidateWritingAndReadingWithKVMutationDeltaStream) {
  std::stringstream string_stream;
  auto record_writer = CreateDeltaRecordStreamWriter(string_stream);
  EXPECT_TRUE(record_writer.ok());

  KeyValueMutationRecordStruct expected_kv = GetKeyValueMutationRecord();
  DataRecordStruct data_record = GetDataRecord(expected_kv);
  EXPECT_TRUE((*record_writer)->WriteRecord(data_record).ok())
      << "Failed to write delta record.";
  (*record_writer)->Close();
  EXPECT_FALSE((*record_writer)->IsOpen());
  auto stream_reader_factory =
      StreamRecordReaderFactory<std::string_view>::Create();
  auto stream_reader = stream_reader_factory->CreateReader(string_stream);
  absl::StatusOr<KVFileMetadata> metadata = stream_reader->GetKVFileMetadata();
  EXPECT_TRUE(metadata.ok()) << "Failed to read metadata";
  EXPECT_TRUE(
      stream_reader
          ->ReadStreamRecords(
              [&expected_kv](std::string_view record_string) -> absl::Status {
                const auto* data_record =
                    flatbuffers::GetRoot<DataRecord>(record_string.data());
                EXPECT_EQ(data_record->record_type(),
                          Record::KeyValueMutationRecord);
                const auto kv_record =
                    GetTypedRecordStruct<KeyValueMutationRecordStruct>(
                        *data_record);
                EXPECT_EQ(kv_record, expected_kv);
                return absl::OkStatus();
              })
          .ok());
}

TEST_P(DeltaRecordStreamWriterTest,
       ValidateWritingAndReadingWithUdfConfigDeltaStream) {
  std::stringstream string_stream;
  auto record_writer = CreateDeltaRecordStreamWriter(string_stream);
  EXPECT_TRUE(record_writer.ok());

  UserDefinedFunctionsConfigStruct expected_udf_config =
      GetUserDefinedFunctionsConfig();
  DataRecordStruct data_record = GetDataRecord(expected_udf_config);
  EXPECT_TRUE((*record_writer)->WriteRecord(data_record).ok())
      << "Failed to write delta record.";
  (*record_writer)->Close();
  EXPECT_FALSE((*record_writer)->IsOpen());
  auto stream_reader_factory =
      StreamRecordReaderFactory<std::string_view>::Create();
  auto stream_reader = stream_reader_factory->CreateReader(string_stream);
  absl::StatusOr<KVFileMetadata> metadata = stream_reader->GetKVFileMetadata();
  EXPECT_TRUE(metadata.ok()) << "Failed to read metadata";
  EXPECT_TRUE(
      stream_reader
          ->ReadStreamRecords(
              [&expected_udf_config](
                  std::string_view record_string) -> absl::Status {
                const auto* data_record =
                    flatbuffers::GetRoot<DataRecord>(record_string.data());
                EXPECT_EQ(data_record->record_type(),
                          Record::UserDefinedFunctionsConfig);
                const auto udf_config =
                    GetTypedRecordStruct<UserDefinedFunctionsConfigStruct>(
                        *data_record);
                EXPECT_EQ(udf_config, expected_udf_config);
                return absl::OkStatus();
              })
          .ok());
}

TEST_P(DeltaRecordStreamWriterTest,
       ValidateWritingAndReadingDeltaStreamForSet) {
  std::stringstream string_stream;
  auto record_writer = CreateDeltaRecordStreamWriter(string_stream);
  EXPECT_TRUE(record_writer.ok());

  DataRecordStruct expected = GetDataRecord(GetDeltaSetRecord());
  EXPECT_TRUE((*record_writer)->WriteRecord(expected).ok())
      << "Failed to write delta record.";
  (*record_writer)->Close();
  EXPECT_FALSE((*record_writer)->IsOpen());
  auto stream_reader_factory =
      StreamRecordReaderFactory<std::string_view>::Create();
  auto stream_reader = stream_reader_factory->CreateReader(string_stream);
  absl::StatusOr<KVFileMetadata> metadata = stream_reader->GetKVFileMetadata();
  EXPECT_TRUE(metadata.ok()) << "Failed to read metadata";
  EXPECT_TRUE(
      stream_reader
          ->ReadStreamRecords(
              [&expected](std::string_view record_string) -> absl::Status {
                return DeserializeDataRecord(
                    record_string, [&expected](DataRecordStruct record) {
                      EXPECT_EQ(record, expected);
                      return absl::OkStatus();
                    });
              })
          .ok());
}

TEST(DeltaRecordStreamWriterTest, ValidateWritingFailsAfterClose) {
  std::stringstream string_stream;
  DeltaRecordWriter::Options options{.metadata = GetMetadata()};
  auto record_writer = DeltaRecordStreamWriter<std::stringstream>::Create(
      string_stream, std::move(options));
  EXPECT_TRUE(record_writer.ok());
  (*record_writer)->Close();
  auto status =
      (*record_writer)->WriteRecord(GetDataRecord(GetKeyValueMutationRecord()));
  EXPECT_FALSE(status.ok());
}

}  // namespace
}  // namespace kv_server
