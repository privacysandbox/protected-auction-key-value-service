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

DeltaFileRecordStruct GetDeltaRecord() {
  DeltaFileRecordStruct record;
  record.key = "key";
  record.subkey = "subkey";
  record.value = "value";
  record.logical_commit_time = 1234567890;
  record.mutation_type = DeltaMutationType::Update;
  return record;
}

TEST(CsvDeltaRecordStreamWriterTest, ValidateWritingCsvRecordFromDelta) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  EXPECT_TRUE(record_writer.WriteRecord(GetDeltaRecord()).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  CsvDeltaRecordStreamReader record_reader(string_stream);
  EXPECT_TRUE(record_reader
                  .ReadRecords([](DeltaFileRecordStruct record) {
                    EXPECT_EQ(record, GetDeltaRecord());
                    return absl::OkStatus();
                  })
                  .ok());
}

TEST(CsvDeltaRecordStreamWriterTest,
     ValidateThatWritingUsingClosedWriterFails) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  record_writer.Close();
  EXPECT_FALSE(record_writer.WriteRecord(GetDeltaRecord()).ok());
}

TEST(CsvDeltaRecordStreamWriterTest, ValidateThatFailedRecordsAreRecoverable) {
  DeltaFileRecordStruct recovered_record;
  CsvDeltaRecordStreamWriter<std::stringstream>::Options options;
  options.recovery_function =
      [&recovered_record](DeltaFileRecordStruct failed_record) {
        recovered_record = failed_record;
      };
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream, options);
  record_writer.Close();
  EXPECT_NE(recovered_record, GetDeltaRecord());
  EXPECT_FALSE(record_writer.WriteRecord(GetDeltaRecord()).ok());
  EXPECT_EQ(recovered_record, GetDeltaRecord());
}

TEST(CsvDeltaRecordStreamWriterTest, ValidateWritingDefaultRecord) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  EXPECT_TRUE(record_writer.WriteRecord(DeltaFileRecordStruct{}).ok());
  EXPECT_TRUE(record_writer.Flush().ok());
  CsvDeltaRecordStreamReader record_reader(string_stream);
  EXPECT_TRUE(record_reader
                  .ReadRecords([](DeltaFileRecordStruct record) {
                    EXPECT_EQ(record, DeltaFileRecordStruct{});
                    return absl::OkStatus();
                  })
                  .ok());
}

TEST(CsvDeltaRecordStreamWriterTest, ValidateClosingRecordWriter) {
  std::stringstream string_stream;
  CsvDeltaRecordStreamWriter record_writer(string_stream);
  record_writer.Close();
  EXPECT_FALSE(record_writer.IsOpen());
}

}  // namespace
}  // namespace kv_server
