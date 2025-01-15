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
#include "public/data_loading/readers/riegeli_stream_record_reader_factory.h"
#include "public/data_loading/record_utils.h"
#include "public/test_util/data_record.h"

namespace kv_server {
namespace {

KVFileMetadata GetMetadata() {
  KVFileMetadata metadata;
  return metadata;
}

class DeltaRecordStreamWriterTest
    : public testing::TestWithParam<DeltaRecordWriter::Options> {
 protected:
  void SetUp() override { kv_server::InitMetricsContextMap(); }
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

  auto expected =
      GetNativeDataRecord(GetKVMutationRecord(GetSimpleStringValue()));
  EXPECT_TRUE((*record_writer)->WriteRecord(expected).ok())
      << "Failed to write delta record.";
  (*record_writer)->Close();
  EXPECT_FALSE((*record_writer)->IsOpen());
  auto stream_reader_factory =
      std::make_unique<RiegeliStreamRecordReaderFactory>();
  auto stream_reader = stream_reader_factory->CreateReader(string_stream);
  absl::StatusOr<KVFileMetadata> metadata = stream_reader->GetKVFileMetadata();
  EXPECT_TRUE(metadata.ok()) << "Failed to read metadata";
  EXPECT_TRUE(
      stream_reader
          ->ReadStreamRecords(
              [&expected](std::string_view record_string) -> absl::Status {
                const auto* data_record =
                    flatbuffers::GetRoot<DataRecord>(record_string.data());
                EXPECT_EQ(*data_record->UnPack(), expected);
                return absl::OkStatus();
              })
          .ok());
}

TEST_P(DeltaRecordStreamWriterTest,
       ValidateWritingAndReadingWithUdfConfigDeltaStream) {
  std::stringstream string_stream;
  auto record_writer = CreateDeltaRecordStreamWriter(string_stream);
  EXPECT_TRUE(record_writer.ok());

  auto expected =
      GetNativeDataRecord(GetKVMutationRecord(GetUserDefinedFunctionsConfig()));
  EXPECT_TRUE((*record_writer)->WriteRecord(expected).ok())
      << "Failed to write delta record.";
  (*record_writer)->Close();
  EXPECT_FALSE((*record_writer)->IsOpen());
  auto stream_reader_factory =
      std::make_unique<RiegeliStreamRecordReaderFactory>();
  auto stream_reader = stream_reader_factory->CreateReader(string_stream);
  absl::StatusOr<KVFileMetadata> metadata = stream_reader->GetKVFileMetadata();
  EXPECT_TRUE(metadata.ok()) << "Failed to read metadata";
  EXPECT_TRUE(
      stream_reader
          ->ReadStreamRecords(
              [&expected](std::string_view record_string) -> absl::Status {
                const auto* data_record =
                    flatbuffers::GetRoot<DataRecord>(record_string.data());
                EXPECT_EQ(*data_record->UnPack(), expected);
                return absl::OkStatus();
              })
          .ok());
}

TEST_P(DeltaRecordStreamWriterTest,
       ValidateWritingAndReadingDeltaStreamForSet) {
  std::stringstream string_stream;
  auto record_writer = CreateDeltaRecordStreamWriter(string_stream);
  EXPECT_TRUE(record_writer.ok());

  const std::vector<std::string_view> values{
      "elem1",
      "elem2",
      "elem3",
  };
  auto expected =
      GetNativeDataRecord(GetKVMutationRecord(GetStringSetValue(values)));
  EXPECT_TRUE((*record_writer)->WriteRecord(expected).ok())
      << "Failed to write delta record.";
  (*record_writer)->Close();
  EXPECT_FALSE((*record_writer)->IsOpen());
  auto stream_reader_factory =
      std::make_unique<RiegeliStreamRecordReaderFactory>();
  auto stream_reader = stream_reader_factory->CreateReader(string_stream);
  absl::StatusOr<KVFileMetadata> metadata = stream_reader->GetKVFileMetadata();
  EXPECT_TRUE(metadata.ok()) << "Failed to read metadata";
  EXPECT_TRUE(
      stream_reader
          ->ReadStreamRecords(
              [&expected](std::string_view record_string) -> absl::Status {
                const auto* data_record =
                    flatbuffers::GetRoot<DataRecord>(record_string.data());
                EXPECT_EQ(*data_record->UnPack(), expected);
                return absl::OkStatus();
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
  auto status = (*record_writer)
                    ->WriteRecord(GetNativeDataRecord(
                        GetKVMutationRecord(GetSimpleStringValue())));
  EXPECT_FALSE(status.ok());
}

}  // namespace
}  // namespace kv_server
