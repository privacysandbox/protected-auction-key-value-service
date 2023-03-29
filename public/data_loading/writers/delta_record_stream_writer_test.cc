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

DeltaFileRecordStruct GetDeltaRecord() {
  DeltaFileRecordStruct record;
  record.key = "key";
  record.value = "value";
  record.logical_commit_time = 1234567890;
  record.mutation_type = DeltaMutationType::Update;
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

INSTANTIATE_TEST_CASE_P(
    ValidMetadata, DeltaRecordStreamWriterTest,
    testing::Values(DeltaRecordWriter::Options{.enable_compression = false,
                                               .metadata = GetMetadata()},
                    DeltaRecordWriter::Options{.enable_compression = true,
                                               .metadata = GetMetadata()}));

TEST_P(DeltaRecordStreamWriterTest, ValidateWritingAndReadingDeltaStream) {
  std::stringstream string_stream;
  auto record_writer = CreateDeltaRecordStreamWriter(string_stream);
  EXPECT_TRUE(record_writer.ok());
  EXPECT_TRUE((*record_writer)->WriteRecord(GetDeltaRecord()).ok())
      << "Failed to write delta record.";
  (*record_writer)->Close();
  EXPECT_FALSE((*record_writer)->IsOpen());
  auto stream_reader_factory =
      StreamRecordReaderFactory<std::string_view>::Create();
  auto stream_reader = stream_reader_factory->CreateReader(string_stream);
  absl::StatusOr<KVFileMetadata> metadata = stream_reader->GetKVFileMetadata();
  EXPECT_TRUE(metadata.ok()) << "Failed to read metadata";
  EXPECT_TRUE(stream_reader
                  ->ReadStreamRecords(
                      [](std::string_view record_string) -> absl::Status {
                        DeltaFileRecordStruct record;
                        auto fbs_record = flatbuffers::GetRoot<DeltaFileRecord>(
                            record_string.data());
                        record.key = fbs_record->key()->string_view();
                        record.value = fbs_record->value()->string_view();
                        record.mutation_type = fbs_record->mutation_type();
                        record.logical_commit_time =
                            fbs_record->logical_commit_time();
                        EXPECT_EQ(record, GetDeltaRecord());
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
  EXPECT_DEATH(auto status = (*record_writer)->WriteRecord(GetDeltaRecord()),
               "terminate called after throwing an instance of "
               "'std::bad_function_call'");
}

}  // namespace
}  // namespace kv_server
