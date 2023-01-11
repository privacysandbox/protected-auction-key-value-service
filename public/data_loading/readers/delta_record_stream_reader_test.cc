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

#include "public/data_loading/readers/delta_record_stream_reader.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/data_loading/records_utils.h"
#include "public/data_loading/writers/delta_record_stream_writer.h"

namespace kv_server {
namespace {

KVFileMetadata GetMetadata() {
  KVFileMetadata metadata;
  metadata.set_key_namespace(KeyNamespace_Enum_KEYS);
  return metadata;
}

DeltaFileRecordStruct GetDeltaRecord() {
  DeltaFileRecordStruct record;
  record.key = "key";
  record.subkey = "subkey";
  record.value = "value";
  record.logical_commit_time = 1234567890;
  record.mutation_type = DeltaMutationType::Update;
  return record;
}

TEST(DeltaRecordStreamReaderTest, ValidateReadingRecords) {
  std::stringstream string_stream;
  auto record_writer = DeltaRecordStreamWriter<>::Create(
      string_stream, DeltaRecordWriter::Options{.metadata = GetMetadata()});
  EXPECT_TRUE(record_writer.ok());
  EXPECT_TRUE((*record_writer)->WriteRecord(GetDeltaRecord()).ok());
  (*record_writer)->Close();
  DeltaRecordStreamReader record_reader(string_stream);
  EXPECT_TRUE(record_reader
                  .ReadRecords([](DeltaFileRecordStruct record) {
                    EXPECT_EQ(record, GetDeltaRecord());
                    return absl::OkStatus();
                  })
                  .ok());
}

TEST(DeltaRecordStreamReaderTest, ValidateReadingRecordCallsRecordCallback) {
  std::stringstream string_stream;
  auto record_writer = DeltaRecordStreamWriter<>::Create(
      string_stream, DeltaRecordWriter::Options{.metadata = GetMetadata()});
  EXPECT_TRUE(record_writer.ok());
  EXPECT_TRUE((*record_writer)->WriteRecord(GetDeltaRecord()).ok());
  EXPECT_TRUE((*record_writer)->WriteRecord(GetDeltaRecord()).ok());
  EXPECT_TRUE((*record_writer)->WriteRecord(GetDeltaRecord()).ok());
  EXPECT_TRUE((*record_writer)->WriteRecord(GetDeltaRecord()).ok());
  (*record_writer)->Close();
  DeltaRecordStreamReader record_reader(string_stream);
  testing::MockFunction<absl::Status(DeltaFileRecordStruct)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(4)
      .WillRepeatedly([](DeltaFileRecordStruct record) {
        EXPECT_EQ(record, GetDeltaRecord());
        return absl::OkStatus();
      });
  EXPECT_TRUE(record_reader.ReadRecords(record_callback.AsStdFunction()).ok());
}

}  // namespace
}  // namespace kv_server
