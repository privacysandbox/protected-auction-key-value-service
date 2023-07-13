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

#include "tools/bidding_auction_data_generator/delta_key_value_writer.h"

#include <string>

#include "gtest/gtest.h"
#include "public/data_loading/readers/delta_record_stream_reader.h"

namespace kv_server {
namespace {
constexpr int64_t kTestLogicalCommitTime = 1234567890;
constexpr KeyValueMutationType kTestDeltaMutationType =
    KeyValueMutationType::Update;

KeyValueMutationRecordStruct GetKVMutationRecord() {
  KeyValueMutationRecordStruct record;
  record.key = "key1";
  record.value = R"({"field": "test"})";
  record.logical_commit_time = kTestLogicalCommitTime;
  record.mutation_type = kTestDeltaMutationType;
  return record;
}

absl::flat_hash_map<std::string, std::string> GetTestKeyValueMap() {
  absl::flat_hash_map<std::string, std::string> test_key_value_map;
  test_key_value_map.insert({"key1", R"({"field": "test"})"});
  return test_key_value_map;
}

TEST(DeltaKeyValueWriterTest, ValidateDeltaDataTest) {
  std::stringstream delta_stream;
  auto delta_key_value_writer = DeltaKeyValueWriter::Create(delta_stream);
  EXPECT_TRUE(delta_key_value_writer.ok()) << delta_key_value_writer.status();
  const auto write_status =
      (*delta_key_value_writer)
          ->Write(GetTestKeyValueMap(), kTestLogicalCommitTime,
                  kTestDeltaMutationType);
  EXPECT_TRUE(write_status.ok());
  auto stream_reader_factory =
      StreamRecordReaderFactory<std::string_view>::Create();
  auto stream_reader = stream_reader_factory->CreateReader(delta_stream);
  EXPECT_TRUE(stream_reader
                  ->ReadStreamRecords(
                      [](std::string_view record_string) -> absl::Status {
                        const auto* data_record =
                            flatbuffers::GetRoot<DataRecord>(
                                record_string.data());
                        EXPECT_EQ(data_record->record_type(),
                                  Record::KeyValueMutationRecord);
                        const auto kv_record =
                            GetTypedRecordStruct<KeyValueMutationRecordStruct>(
                                *data_record);
                        EXPECT_EQ(kv_record, GetKVMutationRecord());
                        return absl::OkStatus();
                      })
                  .ok());
}
}  // namespace
}  // namespace kv_server
