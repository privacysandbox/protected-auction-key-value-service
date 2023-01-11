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

#include "gtest/gtest.h"
#include "public/data_loading/csv/csv_delta_record_stream_writer.h"
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

TEST(CsvDeltaRecordStreamReaderTest, ValidateReadingAndWritingRecords) {
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

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingCsvRecordsWithInvalidTimestamps) {
  const char invalid_data[] =
      "key,subkey,value,mutation_type,logical_commit_time\n\"key\",\"subkey\","
      "\"value\",\"Update\",\"invalid_time\"\n";
  std::stringstream csv_stream;
  csv_stream.str(invalid_data);
  CsvDeltaRecordStreamReader record_reader(csv_stream);
  absl::Status status = record_reader.ReadRecords(
      [](const DeltaFileRecordStruct&) { return absl::OkStatus(); });
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_STREQ(std::string(status.message()).c_str(),
               "Cannot convert timestamp:invalid_time to a number.")
      << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
}

TEST(CsvDeltaRecordStreamReaderTest,
     ValidateReadingCsvRecordsWithInvalidMutation) {
  const char invalid_data[] =
      "key,subkey,value,mutation_type,logical_commit_time\n\"key\",\"subkey\","
      "\"value\",\"invalid_mutation\",\"1000000\"\n";
  std::stringstream csv_stream;
  csv_stream.str(invalid_data);
  CsvDeltaRecordStreamReader record_reader(csv_stream);
  absl::Status status = record_reader.ReadRecords(
      [](DeltaFileRecordStruct) { return absl::OkStatus(); });
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_STREQ(std::string(status.message()).c_str(),
               "Unknown mutation type:invalid_mutation")
      << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
}

}  // namespace
}  // namespace kv_server
