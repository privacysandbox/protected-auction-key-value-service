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

#include "public/data_loading/aggregation/record_aggregator.h"

#include <array>
#include <filesystem>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/data_loading/readers/delta_record_stream_reader.h"
#include "public/data_loading/records_utils.h"
#include "public/test_util/data_record.h"

namespace kv_server {
namespace {

size_t GetRecordKey(const KeyValueMutationRecordT& record) {
  return absl::HashOf(record.key);
}

template <typename ValueT>
KeyValueMutationRecordT GetDeltaRecord(ValueT&& value,
                                       std::string_view key = "key") {
  KeyValueMutationRecordT record;
  record.key = key;
  record.value.Set(std::move(value));
  record.logical_commit_time = 1234567890;
  record.mutation_type = KeyValueMutationType::Update;
  return record;
}

std::string GetTempDbFilepath() {
  return absl::StrFormat("%s/%s.%d.db", std::filesystem::temp_directory_path(),
                         "RecordAggregatorTest", std::rand());
}

class RecordAggregatorTest : public ::testing::TestWithParam<bool> {
 protected:
  absl::StatusOr<std::unique_ptr<RecordAggregator>> CreateAggregator() {
    if ((bool)GetParam()) {
      return RecordAggregator::CreateInMemoryAggregator();
    } else {
      auto db_file = GetTempDbFilepath();
      if (std::filesystem::exists(db_file)) {
        EXPECT_TRUE(std::filesystem::remove(db_file));
      }
      return RecordAggregator::CreateFileBackedAggregator(db_file);
    }
  }
};

INSTANTIATE_TEST_SUITE_P(IsInMemoryBacked, RecordAggregatorTest,
                         testing::Values(true, false));

TEST_P(RecordAggregatorTest, ValidateReadRecord) {
  auto record_aggregator = RecordAggregatorTest::CreateAggregator();
  auto status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  auto record = GetDeltaRecord(GetSimpleStringValue());
  status =
      (*record_aggregator)->InsertOrUpdateRecord(GetRecordKey(record), record);
  EXPECT_TRUE(status.ok()) << status;
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback1;
  EXPECT_CALL(record_callback1, Call)
      .Times(1)
      .WillRepeatedly([](KeyValueMutationRecordT record) {
        EXPECT_EQ(record, GetDeltaRecord(GetSimpleStringValue()));
        return absl::OkStatus();
      });
  status =
      (*record_aggregator)
          ->ReadRecord(GetRecordKey(record), record_callback1.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
  // We don't expect calls to our callback for records that do not exist.
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback2;
  EXPECT_CALL(record_callback2, Call).Times(0);
  status = (*record_aggregator)
               ->ReadRecord(std::hash<std::string>{}("non-existing-record-key"),
                            record_callback2.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST_P(RecordAggregatorTest, ValidateDeleteRecord) {
  auto record_aggregator = RecordAggregatorTest::CreateAggregator();
  auto status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  auto record = GetDeltaRecord(GetSimpleStringValue());
  status =
      (*record_aggregator)->InsertOrUpdateRecord(GetRecordKey(record), record);
  EXPECT_TRUE(status.ok()) << status;
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback1;
  EXPECT_CALL(record_callback1, Call)
      .Times(1)
      .WillRepeatedly([](KeyValueMutationRecordT record) {
        EXPECT_EQ(record, GetDeltaRecord(GetSimpleStringValue()));
        return absl::OkStatus();
      });
  status =
      (*record_aggregator)
          ->ReadRecord(GetRecordKey(record), record_callback1.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
  status = (*record_aggregator)->DeleteRecord(GetRecordKey(record));
  EXPECT_TRUE(status.ok()) << status;
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback2;
  EXPECT_CALL(record_callback2, Call).Times(0);
  status =
      (*record_aggregator)
          ->ReadRecord(GetRecordKey(record), record_callback2.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST_P(RecordAggregatorTest, ValidateDeleteRecords) {
  auto record_aggregator = RecordAggregatorTest::CreateAggregator();
  auto status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  auto record = GetDeltaRecord(GetSimpleStringValue());
  status =
      (*record_aggregator)->InsertOrUpdateRecord(GetRecordKey(record), record);
  EXPECT_TRUE(status.ok()) << status;
  status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback;
  EXPECT_CALL(record_callback, Call).Times(0);
  status =
      (*record_aggregator)
          ->ReadRecord(GetRecordKey(record), record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST_P(RecordAggregatorTest, ValidateInsertingNonExistingRecord) {
  auto record_aggregator = RecordAggregatorTest::CreateAggregator();
  auto status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  auto record = GetDeltaRecord(GetSimpleStringValue());
  status =
      (*record_aggregator)->InsertOrUpdateRecord(GetRecordKey(record), record);
  EXPECT_TRUE(status.ok()) << status;
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(1)
      .WillRepeatedly([](KeyValueMutationRecordT record) {
        EXPECT_EQ(record, GetDeltaRecord(GetSimpleStringValue()));
        return absl::OkStatus();
      });
  status =
      (*record_aggregator)
          ->ReadRecord(GetRecordKey(record), record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST_P(RecordAggregatorTest, ValidateInsertingMoreRecentRecord) {
  auto record_aggregator = RecordAggregatorTest::CreateAggregator();
  auto status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  auto record = GetDeltaRecord(GetSimpleStringValue());
  status =
      (*record_aggregator)->InsertOrUpdateRecord(GetRecordKey(record), record);
  EXPECT_TRUE(status.ok()) << status;
  // Update record to be more recent and verify that updates are reflected in
  // stored record.
  record.logical_commit_time = record.logical_commit_time + 1;
  record.value.Set(GetSimpleStringValue("updated_value"));
  status =
      (*record_aggregator)->InsertOrUpdateRecord(GetRecordKey(record), record);
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(1)
      .WillRepeatedly([&](KeyValueMutationRecordT existing_record) {
        EXPECT_EQ(existing_record, record);
        return absl::OkStatus();
      });
  status =
      (*record_aggregator)
          ->ReadRecord(GetRecordKey(record), record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST_P(RecordAggregatorTest, ValidateInsertingOlderRecord) {
  auto record_aggregator = RecordAggregatorTest::CreateAggregator();
  auto status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  auto record = GetDeltaRecord(GetSimpleStringValue());
  status =
      (*record_aggregator)->InsertOrUpdateRecord(GetRecordKey(record), record);
  EXPECT_TRUE(status.ok()) << status;
  // Update record to be older and verify that stored record is not updated.
  record.logical_commit_time = record.logical_commit_time - 1;
  record.value.Set(GetSimpleStringValue("updated_value"));
  status =
      (*record_aggregator)->InsertOrUpdateRecord(GetRecordKey(record), record);
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(1)
      .WillRepeatedly([](KeyValueMutationRecordT existing_record) {
        EXPECT_EQ(existing_record, GetDeltaRecord(GetSimpleStringValue()));
        return absl::OkStatus();
      });
  status =
      (*record_aggregator)
          ->ReadRecord(GetRecordKey(record), record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST_P(RecordAggregatorTest, ValidateInsertingUpdatedRecordWithSameTimestamp) {
  auto record_aggregator = RecordAggregatorTest::CreateAggregator();
  auto status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  auto record = GetDeltaRecord(GetSimpleStringValue());
  status =
      (*record_aggregator)->InsertOrUpdateRecord(GetRecordKey(record), record);
  EXPECT_TRUE(status.ok()) << status;
  // Update record and verify that stored record is updated.
  // Since updated record has the same timestamp, new values should be
  // reflected in store.
  record.value.Set(GetSimpleStringValue("updated_value"));
  status =
      (*record_aggregator)->InsertOrUpdateRecord(GetRecordKey(record), record);
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(1)
      .WillRepeatedly([&](KeyValueMutationRecordT existing_record) {
        EXPECT_EQ(existing_record, record);
        return absl::OkStatus();
      });
  status =
      (*record_aggregator)
          ->ReadRecord(GetRecordKey(record), record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST_P(RecordAggregatorTest, ValidateInsertingMultipleRecords) {
  auto record_aggregator = RecordAggregatorTest::CreateAggregator();
  auto status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  auto record = GetDeltaRecord(GetSimpleStringValue());
  status =
      (*record_aggregator)->InsertOrUpdateRecord(GetRecordKey(record), record);
  EXPECT_TRUE(status.ok()) << status;
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback1;
  EXPECT_CALL(record_callback1, Call)
      .Times(1)
      .WillRepeatedly([](KeyValueMutationRecordT existing_record) {
        EXPECT_EQ(existing_record, GetDeltaRecord(GetSimpleStringValue()));
        return absl::OkStatus();
      });
  status =
      (*record_aggregator)
          ->ReadRecord(GetRecordKey(record), record_callback1.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
  std::string updated_key = absl::StrCat("another-", record.key);
  record.key = updated_key;
  status =
      (*record_aggregator)->InsertOrUpdateRecord(GetRecordKey(record), record);
  EXPECT_TRUE(status.ok()) << status;
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback2;
  EXPECT_CALL(record_callback2, Call)
      .Times(1)
      .WillRepeatedly([&](KeyValueMutationRecordT existing_record) {
        EXPECT_EQ(existing_record, record);
        return absl::OkStatus();
      });
  status =
      (*record_aggregator)
          ->ReadRecord(GetRecordKey(record), record_callback2.AsStdFunction());
}

TEST_P(RecordAggregatorTest, ValidateInsertingInvalidRecords) {
  auto record_aggregator = RecordAggregatorTest::CreateAggregator();
  auto status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  KeyValueMutationRecordT record;
  status = (*record_aggregator)
               ->InsertOrUpdateRecord(std::hash<std::string>{}("key1"), record);
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  record.key = "key1";
  status = (*record_aggregator)
               ->InsertOrUpdateRecord(std::hash<std::string>{}("key1"), record);
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  record.value.Set(GetSimpleStringValue("value1"));
  status = (*record_aggregator)
               ->InsertOrUpdateRecord(std::hash<std::string>{}("key1"), record);
  EXPECT_TRUE(status.ok()) << status;
}

TEST_P(RecordAggregatorTest, ValidateReadingRecords) {
  auto record_aggregator = RecordAggregatorTest::CreateAggregator();
  auto status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  constexpr std::array<std::string_view, 5> kRecordKeys = {
      "key1", "key2", "key3", "key4", "key5"};
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback;
  for (std::string_view key : kRecordKeys) {
    auto record = GetDeltaRecord(GetSimpleStringValue(), key);
    auto status = (*record_aggregator)
                      ->InsertOrUpdateRecord(GetRecordKey(record), record);
    EXPECT_TRUE(status.ok()) << status;
    // record_callback should be called exactly as many times as the number of
    // inserted records and each call should be with a record that matches an
    // inserted record.
    EXPECT_CALL(record_callback, Call(record))
        .WillOnce([](KeyValueMutationRecordT) { return absl::OkStatus(); });
  }
  EXPECT_TRUE(
      (*record_aggregator)->ReadRecords(record_callback.AsStdFunction()).ok());
}

TEST_P(RecordAggregatorTest, ValidateReadingRecordsFromEmptyAggregator) {
  auto record_aggregator = RecordAggregatorTest::CreateAggregator();
  auto status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback;
  EXPECT_CALL(record_callback, Call).Times(0);
  EXPECT_TRUE(
      (*record_aggregator)->ReadRecords(record_callback.AsStdFunction()).ok());
}

TEST_P(RecordAggregatorTest, ValidateReadingRecordsWhenCallbackFails) {
  auto record_aggregator = RecordAggregatorTest::CreateAggregator();
  auto status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  auto record = GetDeltaRecord(GetSimpleStringValue());
  status =
      (*record_aggregator)->InsertOrUpdateRecord(GetRecordKey(record), record);
  EXPECT_TRUE(status.ok()) << status;
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(1)
      .WillOnce([&](KeyValueMutationRecordT record) {
        return absl::InvalidArgumentError("Callback failed.");
      });
  status = (*record_aggregator)->ReadRecords(record_callback.AsStdFunction());
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_STREQ(status.message().data(), "Callback failed.");
}

TEST_P(RecordAggregatorTest, ValidateAggregatingRecordsWithSetValues) {
  auto record_aggregator = RecordAggregatorTest::CreateAggregator();
  auto status = (*record_aggregator)->DeleteRecords();
  EXPECT_TRUE(status.ok()) << status;
  testing::MockFunction<absl::Status(KeyValueMutationRecordT)> record_callback;
  auto record1 = GetDeltaRecord(GetStringSetValue(std::vector<std::string_view>{
                                    "value1", "value2", "value3", "value4"}),
                                "key1");
  status = (*record_aggregator)
               ->InsertOrUpdateRecord(GetRecordKey(record1), record1);
  EXPECT_TRUE(status.ok()) << status;
  auto record2 = GetDeltaRecord(GetStringSetValue(std::vector<std::string_view>{
                                    "value3", "value4", "value5"}),
                                "key1");
  status = (*record_aggregator)
               ->InsertOrUpdateRecord(GetRecordKey(record2), record2);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_CALL(record_callback, Call)
      .WillOnce([](KeyValueMutationRecordT record) {
        EXPECT_THAT(record.value.AsStringSet()->value,
                    testing::UnorderedElementsAre("value1", "value2", "value3",
                                                  "value4", "value5"));
        return absl::OkStatus();
      });
  EXPECT_TRUE(
      (*record_aggregator)->ReadRecords(record_callback.AsStdFunction()).ok());
}

}  // namespace
}  // namespace kv_server
