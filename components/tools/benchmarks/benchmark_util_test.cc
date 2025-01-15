/*
 * Copyright 2023 Google LLC
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

#include "components/tools/benchmarks/benchmark_util.h"

#include <sstream>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/data_loading/readers/delta_record_stream_reader.h"

namespace kv_server::benchmark {
namespace {

TEST(BenchmarkUtilTest, VerifyParseInt64ListWorksWithValidList) {
  auto num_list = ParseInt64List({
      "16",
      "32",
      "64",
  });
  EXPECT_TRUE(num_list.ok()) << num_list.status();
  EXPECT_THAT(*num_list, testing::ElementsAre(16, 32, 64));
}

TEST(BenchmarkUtilTest, VerifyParseInt64ListFailsWithInvalidList) {
  auto num_list = ParseInt64List({
      "test",
      "32",
      "64",
  });
  EXPECT_FALSE(num_list.ok()) << num_list.status();
  EXPECT_THAT(num_list.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(BenchmarkUtilTest, VerifyWriteRecords) {
  std::stringstream data_stream;
  int64_t num_records = 1000;
  int64_t record_size = 2048;
  auto status = WriteRecords(num_records, record_size, data_stream);
  EXPECT_TRUE(status.ok()) << status;
  DeltaRecordStreamReader record_reader(data_stream);
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(num_records)
      .WillRepeatedly([record_size](const DataRecord& data_record) {
        DataRecordT data_record_struct;
        data_record.UnPackTo(&data_record_struct);
        EXPECT_EQ(data_record_struct.record.type,
                  Record::KeyValueMutationRecord);
        const auto kv_record =
            *data_record_struct.record.AsKeyValueMutationRecord();
        EXPECT_EQ(kv_record.value.type, Value::StringValue);
        const auto value = *kv_record.value.AsStringValue();
        EXPECT_EQ(value.value.size(), record_size);
        return absl::OkStatus();
      });
  status = record_reader.ReadRecords(record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

}  // namespace
}  // namespace kv_server::benchmark
