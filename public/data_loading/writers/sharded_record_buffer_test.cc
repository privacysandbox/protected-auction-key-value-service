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

#include "public/data_loading/writers/sharded_record_buffer.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/data_loading/readers/delta_record_stream_reader.h"
#include "public/sharding/sharding_function.h"

namespace kv_server {
namespace {

KeyValueMutationRecordStruct GetKVMutationRecord(std::string_view key) {
  return KeyValueMutationRecordStruct{
      .mutation_type = KeyValueMutationType::Update,
      .logical_commit_time = 1234567890,
      .key = key,
      .value = "value",
  };
}

DataRecordStruct GetDataRecord(const RecordT& record) {
  DataRecordStruct data_record;
  data_record.record = record;
  return data_record;
}

void ValidateRecordStream(std::istream& record_stream) {
  testing::MockFunction<absl::Status(DataRecordStruct)> record_callback;
  EXPECT_CALL(record_callback, Call).Times(0);
  DeltaRecordStreamReader record_reader(record_stream);
  auto status = record_reader.ReadRecords(record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

void ValidateRecordStream(const std::vector<std::string_view> keys,
                          std::istream& record_stream) {
  testing::MockFunction<absl::Status(DataRecordStruct)> record_callback;
  if (keys.size() == 1) {
    EXPECT_CALL(record_callback, Call)
        .Times(1)
        .WillOnce([&keys](DataRecordStruct data_record) {
          if (std::holds_alternative<KeyValueMutationRecordStruct>(
                  data_record.record)) {
            auto kv_record =
                std::get<KeyValueMutationRecordStruct>(data_record.record);
            EXPECT_EQ(kv_record.key, keys[0]);
          }
          return absl::OkStatus();
        });
  } else {
    EXPECT_CALL(record_callback, Call)
        .Times(keys.size())
        .WillRepeatedly([&keys](DataRecordStruct data_record) {
          if (std::holds_alternative<KeyValueMutationRecordStruct>(
                  data_record.record)) {
            auto kv_record =
                std::get<KeyValueMutationRecordStruct>(data_record.record);
            auto key_iter = std::find(keys.begin(), keys.end(), kv_record.key);
            EXPECT_TRUE(key_iter != keys.end());
          }
          return absl::OkStatus();
        });
  }
  DeltaRecordStreamReader record_reader(record_stream);
  auto status = record_reader.ReadRecords(record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(ShardedRecordBufferTest, ValidateCreatingBuffer) {
  auto buffer = ShardedRecordBuffer::Create(-5);
  EXPECT_FALSE(buffer.ok()) << buffer.status();
  EXPECT_EQ(buffer.status().code(), absl::StatusCode::kInvalidArgument);
  buffer = ShardedRecordBuffer::Create(7);
  EXPECT_TRUE(buffer.ok()) << buffer.status();
}

TEST(ShardedRecordBufferTest, ValidateAddingAndReadingRecords) {
  int num_shards = 7;
  auto keys = std::vector<std::string_view>{
      "key1", "key2", "key3", "key4", "key5", "key6", "key7",
  };
  ShardingFunction sharding_func(/*seed=*/"");
  auto record_buffer = ShardedRecordBuffer::Create(num_shards, sharding_func);
  EXPECT_TRUE(record_buffer.ok()) << record_buffer.status();
  for (const auto& key : keys) {
    auto status =
        (*record_buffer)->AddRecord(GetDataRecord(GetKVMutationRecord(key)));
    EXPECT_TRUE(status.ok()) << status;
  }
  auto status = (*record_buffer)->Flush();
  EXPECT_TRUE(status.ok()) << status;

  // shard 2 and 4 have no records.
  for (auto shard_id : std::vector<int>{2, 4}) {
    auto shard_stream = (*record_buffer)->GetShardRecordStream(shard_id);
    EXPECT_TRUE(shard_stream.ok()) << shard_stream.status();
    ValidateRecordStream(**shard_stream);
  }
  // {key1,key5}=5, {key2,key7}=6, key3=1, key4=0, key6=3
  auto shard_stream = (*record_buffer)->GetShardRecordStream(5);
  EXPECT_TRUE(shard_stream.ok()) << shard_stream.status();
  ValidateRecordStream({"key1", "key5"}, **shard_stream);

  shard_stream = (*record_buffer)->GetShardRecordStream(6);
  EXPECT_TRUE(shard_stream.ok()) << shard_stream.status();
  ValidateRecordStream({"key2", "key7"}, **shard_stream);

  shard_stream = (*record_buffer)->GetShardRecordStream(1);
  EXPECT_TRUE(shard_stream.ok()) << shard_stream.status();
  ValidateRecordStream({"key3"}, **shard_stream);

  shard_stream = (*record_buffer)->GetShardRecordStream(0);
  EXPECT_TRUE(shard_stream.ok()) << shard_stream.status();
  ValidateRecordStream({"key4"}, **shard_stream);

  shard_stream = (*record_buffer)->GetShardRecordStream(3);
  EXPECT_TRUE(shard_stream.ok()) << shard_stream.status();
  ValidateRecordStream({"key6"}, **shard_stream);
}

}  // namespace
}  // namespace kv_server
