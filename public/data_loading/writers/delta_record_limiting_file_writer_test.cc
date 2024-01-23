/*
 * Copyright 2024 Google LLC
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

#include "public/data_loading/writers/delta_record_limiting_file_writer.h"

#include <filesystem>
#include <fstream>

#include "gtest/gtest.h"
#include "public/data_loading/readers/delta_record_stream_reader.h"

namespace kv_server {
namespace {

using kv_server::DataRecordStruct;
using kv_server::KeyValueMutationRecordStruct;
using privacy_sandbox::server_common::TelemetryProvider;

const int file_size_limit_bytes = 10000;

void Write(std::string file_name, int num_records) {
  auto maybe_record_writer = kv_server::DeltaRecordLimitingFileWriter::Create(
      file_name, {}, file_size_limit_bytes);
  EXPECT_TRUE(maybe_record_writer.ok());
  auto record_writer = std::move(*maybe_record_writer);
  int cur_record = 1;
  while (cur_record <= num_records) {
    const std::string key = "key";
    const std::string value = "value";
    auto kv_mutation_record = KeyValueMutationRecordStruct{
        .mutation_type = kv_server::KeyValueMutationType::Update,
        .logical_commit_time = absl::ToUnixSeconds(absl::Now()),
        .key = key,
        .value = value,
    };
    auto data_record =
        DataRecordStruct{.record = std::move(kv_mutation_record)};
    auto result = record_writer->WriteRecord(data_record);
    if (!result.ok()) {
      return;
    }
    cur_record++;
  }
}

int Read(std::ifstream input_stream) {
  auto record_reader =
      kv_server::DeltaRecordStreamReader<std::ifstream>(input_stream);
  int64_t records_count = 0;
  absl::Status status = record_reader.ReadRecords(
      [&records_count](const kv_server::DataRecord& data_record) {
        records_count++;
        std::unique_ptr<kv_server::DataRecordT> data_record_native(
            data_record.UnPack());
        auto [fbs_buffer, serialized_string_view] =
            Serialize(*data_record_native);
        return kv_server::DeserializeDataRecord(
            serialized_string_view,
            [&records_count](const kv_server::DataRecordStruct& data_record_2) {
              auto kv_record =
                  std::get<KeyValueMutationRecordStruct>(data_record_2.record);
              EXPECT_EQ(kv_record.key, "key");
              return absl::OkStatus();
            });
      });
  EXPECT_TRUE(status.ok());
  return records_count;
}

class ParametrizedDeltaRecordLimitingFileWriterTest
    : public ::testing::TestWithParam<int> {};

INSTANTIATE_TEST_SUITE_P(NumberOfRecords,
                         ParametrizedDeltaRecordLimitingFileWriterTest,
                         testing::Values(0, 1, 50));

TEST_P(ParametrizedDeltaRecordLimitingFileWriterTest, WriteRead) {
  char file_name[] = "/tmp/fileXXXXXX";
  int fd = mkstemp(file_name);
  int num_records = GetParam();
  Write(file_name, num_records);
  int file_size_bytes = std::filesystem::file_size(file_name);
  int records_read = Read(std::ifstream(file_name));
  // close and unlink, which will delete the tmp file
  unlink(file_name);
  close(fd);
  EXPECT_TRUE(file_size_limit_bytes >= file_size_bytes);
  EXPECT_EQ(records_read, num_records);
}

TEST(DeltaRecordLimitingFileWriterTest, WriteHitTheLimitRead) {
  char file_name[] = "/tmp/fileXXXXXX";
  int fd = mkstemp(file_name);
  // The number of records was empricially picked to exceed the size limit
  // imposed on the file
  int num_records = 100;
  Write(file_name, num_records);
  int file_size_bytes = std::filesystem::file_size(file_name);
  int records_read = Read(std::ifstream(file_name));
  // close and unlink, which will delete the tmp file
  unlink(file_name);
  close(fd);
  EXPECT_TRUE(file_size_limit_bytes >= file_size_bytes);
  // the number is based on the imperical measurements.
  // making sure that we wrote and read some records
  EXPECT_GT(records_read, 50);
}

}  // namespace
}  // namespace kv_server
