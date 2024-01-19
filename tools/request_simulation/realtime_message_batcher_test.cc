// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tools/request_simulation/realtime_message_batcher.h"

#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "gtest/gtest.h"
#include "public/data_loading/readers/riegeli_stream_record_reader_factory.h"

namespace kv_server {
namespace {

absl::StatusOr<std::vector<kv_server::KeyValueMutationRecordStruct>> Convert(
    RealtimeMessage rt) {
  std::vector<kv_server::KeyValueMutationRecordStruct> rows;
  std::string string_decoded;
  absl::Base64Unescape(rt.message, &string_decoded);
  std::istringstream is(string_decoded);
  auto delta_stream_reader_factory =
      std::make_unique<kv_server::RiegeliStreamRecordReaderFactory>();
  auto record_reader = delta_stream_reader_factory->CreateReader(is);
  if (rt.shard_num.has_value()) {
    auto metadata = record_reader->GetKVFileMetadata();
    if (!metadata.ok() || !metadata->has_sharding_metadata()) {
      return absl::InvalidArgumentError("Missing metadata");
    }
    EXPECT_EQ(metadata->sharding_metadata().shard_num(), rt.shard_num.value());
  }
  auto result = record_reader->ReadStreamRecords([&rows](std::string_view raw) {
    const auto* data_record = flatbuffers::GetRoot<DataRecord>(raw.data());
    const auto kv_record =
        GetTypedRecordStruct<KeyValueMutationRecordStruct>(*data_record);
    EXPECT_TRUE(absl::StartsWith(kv_record.key, "key"));
    rows.emplace_back(kv_record);
    return absl::OkStatus();
  });
  return rows;
}

void Write(std::queue<RealtimeMessage>& realtime_messages, int num_records,
           int num_shards) {
  int message_size_kb = 10;
  absl::Mutex mutex;
  auto batcher_maybe = RealtimeMessageBatcher::Create(
      realtime_messages, mutex, num_shards, message_size_kb);
  ASSERT_TRUE(batcher_maybe.ok());
  auto batcher = std::move(*batcher_maybe);
  int cur_record = 1;
  while (cur_record <= num_records) {
    const std::string key = absl::StrCat("key", std::to_string(cur_record));
    const std::string value = "value";
    auto kv_mutation_record = KeyValueMutationRecordStruct{
        .mutation_type = kv_server::KeyValueMutationType::Update,
        .logical_commit_time = 1232,
        .key = key,
        .value = value,
    };
    auto result = batcher->Insert(kv_mutation_record);
    if (!result.ok()) {
      return;
    }
    cur_record++;
  }
}

class ParametrizeRealtimeMessageBatcherTest
    : public ::testing::TestWithParam<int> {};

INSTANTIATE_TEST_SUITE_P(NumberOfRecords, ParametrizeRealtimeMessageBatcherTest,
                         testing::Values(0, 1, 50));

TEST_P(ParametrizeRealtimeMessageBatcherTest,
       NonShardedMessageFlushedSuccesfullyOnDestruction) {
  std::queue<RealtimeMessage> realtime_messages;
  int rows_number = GetParam();
  int num_shards = 1;
  ASSERT_NO_FATAL_FAILURE(Write(realtime_messages, rows_number, num_shards));
  ASSERT_EQ(realtime_messages.size(), 1);
  auto message = realtime_messages.front();
  realtime_messages.pop();
  auto maybe_delta_rows = Convert(message);
  ASSERT_TRUE(maybe_delta_rows.ok());
  auto delta_rows = std::move(*maybe_delta_rows);
  ASSERT_EQ(delta_rows.size(), rows_number);
  for (int i = 0; i < rows_number; i++) {
    EXPECT_EQ(delta_rows[i].logical_commit_time, 1232);
  }
}

TEST(RealtimeMessageBatcher, NonShardedMessageFewMessagesWrittenSuccesfully) {
  std::queue<RealtimeMessage> realtime_messages;
  int rows_number = 200;
  int num_shards = 1;
  ASSERT_NO_FATAL_FAILURE(Write(realtime_messages, rows_number, num_shards));
  EXPECT_EQ(realtime_messages.size(), 2);
  while (!realtime_messages.empty()) {
    auto message = realtime_messages.front();
    realtime_messages.pop();
    auto maybe_delta_rows = Convert(message);
    ASSERT_TRUE(maybe_delta_rows.ok());
    auto delta_rows = std::move(*maybe_delta_rows);
    for (int i = 0; i < delta_rows.size(); i++) {
      EXPECT_EQ(delta_rows[i].logical_commit_time, 1232);
    }
  }
}

TEST_P(ParametrizeRealtimeMessageBatcherTest,
       ShardedMessageFlushedSuccesfullyOnDestruction) {
  std::queue<RealtimeMessage> realtime_messages;
  int rows_number = GetParam();
  int num_shards = 2;
  ASSERT_NO_FATAL_FAILURE(
      Write(realtime_messages, rows_number * num_shards, num_shards));
  EXPECT_EQ(realtime_messages.size(), num_shards);
  std::vector<int> message_shards = {1, 1};
  while (!realtime_messages.empty()) {
    auto message = realtime_messages.front();
    realtime_messages.pop();
    auto maybe_delta_rows = Convert(message);
    ASSERT_TRUE(maybe_delta_rows.ok());
    auto delta_rows = std::move(*maybe_delta_rows);
    if (message.shard_num.has_value()) {
      message_shards[message.shard_num.value()]--;
    }
    for (int i = 0; i < delta_rows.size(); i++) {
      EXPECT_EQ(delta_rows[i].logical_commit_time, 1232);
    }
  }
  if (rows_number > 0) {
    for (int i = 0; i < num_shards; i++) {
      EXPECT_EQ(message_shards[i], 0);
    }
  }
}

TEST(RealtimeMessageBatcher,
     ShardedMessagesCrossesTheMessageSizeLimitFlushesSuccesfully) {
  std::queue<RealtimeMessage> realtime_messages;
  int rows_number = 200;
  int num_shards = 2;
  int total_messages = 2;
  ASSERT_NO_FATAL_FAILURE(
      Write(realtime_messages, rows_number * num_shards, num_shards));
  EXPECT_EQ(realtime_messages.size(), num_shards * total_messages);
  std::vector<int> message_shards = {1 * total_messages, 1 * total_messages};
  while (!realtime_messages.empty()) {
    auto message = realtime_messages.front();
    realtime_messages.pop();
    auto maybe_delta_rows = Convert(message);
    ASSERT_TRUE(maybe_delta_rows.ok());
    auto delta_rows = std::move(*maybe_delta_rows);
    if (message.shard_num.has_value()) {
      message_shards[message.shard_num.value()]--;
    }
    for (int i = 0; i < delta_rows.size(); i++) {
      EXPECT_EQ(delta_rows[i].logical_commit_time, 1232);
    }
  }
  if (rows_number > 0) {
    for (int i = 0; i < num_shards; i++) {
      EXPECT_EQ(message_shards[i], 0);
    }
  }
}

}  // namespace
}  // namespace kv_server
