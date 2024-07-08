// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "public/data_loading/record_utils.h"

#include <utility>

#include "absl/hash/hash_testing.h"
#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/data_loading/data_loading_generated.h"

namespace kv_server {
namespace {

TEST(RecordUtilsTest, StringValue) {
  // Serialize
  StringValueT string_native;
  string_native.value = "hi";
  auto [fbs_buffer, serialized_string_view] = Serialize(string_native);

  const StringValue* fbs_record =
      flatbuffers::GetRoot<StringValue>(serialized_string_view.data());
  EXPECT_EQ(fbs_record->value()->string_view(), "hi");
}

TEST(RecordUtilsTest, StringSet) {
  // Serialize
  StringSetT string_set_native;
  string_set_native.value = {"value1", "value2"};
  auto [fbs_buffer, serialized_string_view] = Serialize(string_set_native);

  const StringSet* fbs_record =
      flatbuffers::GetRoot<StringSet>(serialized_string_view.data());
  EXPECT_EQ(fbs_record->value()->Get(0)->string_view(), "value1");
}

TEST(RecordUtilsTest, KeyValueMutationRecordWithStringValue) {
  // Serialize
  KeyValueMutationRecordT kv_mutation_record_native;
  kv_mutation_record_native.key = "key";
  kv_mutation_record_native.logical_commit_time = 5;
  StringValueT string_native;
  string_native.value = "hi";
  kv_mutation_record_native.value.Set(std::move(string_native));
  auto [fbs_buffer, serialized_string_view] =
      Serialize(kv_mutation_record_native);

  // Deserialize
  testing::MockFunction<absl::Status(const KeyValueMutationRecord&)>
      record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(1)
      .WillOnce([](const KeyValueMutationRecord& fbs_record) {
        EXPECT_EQ(fbs_record.key()->string_view(), "key");
        EXPECT_EQ(fbs_record.logical_commit_time(), 5);

        absl::StatusOr<std::string_view> maybe_record_value =
            MaybeGetRecordValue<std::string_view>(fbs_record);
        EXPECT_TRUE(maybe_record_value.ok()) << maybe_record_value.status();
        EXPECT_EQ(*maybe_record_value, "hi");

        return absl::OkStatus();
      });
  auto status = DeserializeRecord(serialized_string_view,
                                  record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(RecordUtilsTest, KeyValueMutationRecordWithStringSetValue) {
  // Serialize
  KeyValueMutationRecordT kv_mutation_record_native;
  kv_mutation_record_native.key = "key";
  kv_mutation_record_native.logical_commit_time = 5;
  StringSetT value_native;
  value_native.value = {"value1", "value2"};
  kv_mutation_record_native.value.Set(std::move(value_native));
  auto [fbs_buffer, serialized_string_view] =
      Serialize(kv_mutation_record_native);

  // Deserialize
  testing::MockFunction<absl::Status(const KeyValueMutationRecord&)>
      record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(1)
      .WillOnce([](const KeyValueMutationRecord& fbs_record) {
        EXPECT_EQ(fbs_record.key()->string_view(), "key");
        EXPECT_EQ(fbs_record.logical_commit_time(), 5);

        absl::StatusOr<std::vector<std::string_view>> maybe_record_value =
            MaybeGetRecordValue<std::vector<std::string_view>>(fbs_record);
        EXPECT_TRUE(maybe_record_value.ok()) << maybe_record_value.status();
        EXPECT_THAT(*maybe_record_value,
                    testing::UnorderedElementsAre("value1", "value2"));

        return absl::OkStatus();
      });
  auto status = DeserializeRecord(serialized_string_view,
                                  record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(RecordUtilsTest, DataRecordWithKeyValueMutationRecordWithStringValue) {
  // Serialize
  KeyValueMutationRecordT kv_mutation_record_native;
  kv_mutation_record_native.key = "key";
  kv_mutation_record_native.logical_commit_time = 5;
  StringValueT string_native;
  string_native.value = "hi";
  kv_mutation_record_native.value.Set(std::move(string_native));
  DataRecordT data_record_native;
  data_record_native.record.Set(std::move(kv_mutation_record_native));
  auto [fbs_buffer, serialized_string_view] = Serialize(data_record_native);

  // Deserialize
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(1)
      .WillOnce([](const DataRecord& fbs_record) {
        const KeyValueMutationRecord& kv_mutation_record =
            *fbs_record.record_as_KeyValueMutationRecord();
        EXPECT_EQ(kv_mutation_record.key()->string_view(), "key");
        EXPECT_EQ(kv_mutation_record.logical_commit_time(), 5);

        absl::StatusOr<std::string_view> maybe_record_value =
            MaybeGetRecordValue<std::string_view>(kv_mutation_record);
        EXPECT_TRUE(maybe_record_value.ok()) << maybe_record_value.status();
        EXPECT_EQ(*maybe_record_value, "hi");

        return absl::OkStatus();
      });
  auto status = DeserializeRecord(serialized_string_view,
                                  record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(RecordUtilsTest, DataRecordWithKeyValueMutationRecordWithStringSetValue) {
  // Serialize
  KeyValueMutationRecordT kv_mutation_record_native;
  kv_mutation_record_native.key = "key";
  kv_mutation_record_native.logical_commit_time = 5;
  StringSetT value_native;
  value_native.value = {"value1", "value2"};
  kv_mutation_record_native.value.Set(std::move(value_native));
  DataRecordT data_record_native;
  data_record_native.record.Set(std::move(kv_mutation_record_native));
  auto [fbs_buffer, serialized_string_view] = Serialize(data_record_native);

  // Deserialize
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(1)
      .WillOnce([](const DataRecord& fbs_record) {
        const KeyValueMutationRecord& kv_mutation_record =
            *fbs_record.record_as_KeyValueMutationRecord();

        EXPECT_EQ(kv_mutation_record.key()->string_view(), "key");
        EXPECT_EQ(kv_mutation_record.logical_commit_time(), 5);

        absl::StatusOr<std::vector<std::string_view>> maybe_record_value =
            MaybeGetRecordValue<std::vector<std::string_view>>(
                kv_mutation_record);
        EXPECT_TRUE(maybe_record_value.ok()) << maybe_record_value.status();
        EXPECT_THAT(*maybe_record_value,
                    testing::UnorderedElementsAre("value1", "value2"));

        return absl::OkStatus();
      });
  auto status = DeserializeRecord(serialized_string_view,
                                  record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(RecordUtilsTest, DataRecordWithKeyValueMutationRecordWithUInt32SetValue) {
  // Serialize
  KeyValueMutationRecordT kv_mutation_record_native;
  kv_mutation_record_native.key = "key";
  kv_mutation_record_native.logical_commit_time = 5;
  UInt32SetT value_native;
  value_native.value = {1000, 1001, 1002};
  kv_mutation_record_native.value.Set(std::move(value_native));
  DataRecordT data_record_native;
  data_record_native.record.Set(std::move(kv_mutation_record_native));
  auto [fbs_buffer, serialized_string_view] = Serialize(data_record_native);
  // Deserialize
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(1)
      .WillOnce([](const DataRecord& fbs_record) {
        const KeyValueMutationRecord& kv_mutation_record =
            *fbs_record.record_as_KeyValueMutationRecord();
        EXPECT_EQ(kv_mutation_record.key()->string_view(), "key");
        EXPECT_EQ(kv_mutation_record.logical_commit_time(), 5);
        absl::StatusOr<std::vector<uint32_t>> maybe_record_value =
            MaybeGetRecordValue<std::vector<uint32_t>>(kv_mutation_record);
        EXPECT_TRUE(maybe_record_value.ok()) << maybe_record_value.status();
        EXPECT_THAT(*maybe_record_value,
                    testing::UnorderedElementsAre(1000, 1001, 1002));
        return absl::OkStatus();
      });
  auto status = DeserializeRecord(serialized_string_view,
                                  record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(DataRecordTest, DeserializeDataRecordEmptyRecordFailure) {
  DataRecordT data_record_native;
  auto [fbs_buffer, serialized_string_view] = Serialize(data_record_native);

  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call).Times(0);
  auto status = DeserializeRecord(serialized_string_view,
                                  record_callback.AsStdFunction());
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.message(), "Record not set.");
}

TEST(DataRecordTest,
     DeserializeDataRecord_ToFbsRecord_KVMutation_KeyNotSet_Failure) {
  // Serialize
  KeyValueMutationRecordT kv_mutation_record_native;
  kv_mutation_record_native.logical_commit_time = 5;
  StringValueT string_native;
  string_native.value = "hi";
  kv_mutation_record_native.value.Set(std::move(string_native));
  DataRecordT data_record_native;
  data_record_native.record.Set(std::move(kv_mutation_record_native));
  auto [fbs_buffer, serialized_string_view] = Serialize(data_record_native);

  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call).Times(0);
  auto status = DeserializeRecord(serialized_string_view,
                                  record_callback.AsStdFunction());
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.message(), "Key not set.");
}

TEST(DataRecordTest,
     DeserializeDataRecord_ToFbsRecord_KVMutation_ValueNotSet_Failure) {
  // Serialize
  KeyValueMutationRecordT kv_mutation_record_native;
  kv_mutation_record_native.logical_commit_time = 5;
  kv_mutation_record_native.key = "key";
  DataRecordT data_record_native;
  data_record_native.record.Set(std::move(kv_mutation_record_native));
  auto [fbs_buffer, serialized_string_view] = Serialize(data_record_native);

  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call).Times(0);
  auto status = DeserializeRecord(serialized_string_view,
                                  record_callback.AsStdFunction());
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.message(), "Value not set.");
}

TEST(RecordUtilsTest, DataRecordWithUDFConfig) {
  // Serialize
  UserDefinedFunctionsConfigT udf_config_native;
  udf_config_native.language = UserDefinedFunctionsLanguage::Javascript;
  udf_config_native.code_snippet = "function my_handler(){}";
  udf_config_native.handler_name = "my_handler";
  udf_config_native.logical_commit_time = 5;
  DataRecordT data_record_native;
  data_record_native.record.Set(std::move(udf_config_native));
  auto [fbs_buffer, serialized_string_view] = Serialize(data_record_native);

  // Deserialize
  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(1)
      .WillOnce([](const DataRecord& fbs_record) {
        const UserDefinedFunctionsConfig& udf_config_record =
            *fbs_record.record_as_UserDefinedFunctionsConfig();
        EXPECT_EQ(udf_config_record.code_snippet()->string_view(),
                  "function my_handler(){}");
        EXPECT_EQ(udf_config_record.handler_name()->string_view(),
                  "my_handler");
        EXPECT_EQ(udf_config_record.logical_commit_time(), 5);
        return absl::OkStatus();
      });
  auto status = DeserializeRecord(serialized_string_view,
                                  record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(DataRecordTest, DeserializeRecord_UdfConfig_CodeSnippetNotSet_Failure) {
  // Serialize
  UserDefinedFunctionsConfigT udf_config_native;
  udf_config_native.language = UserDefinedFunctionsLanguage::Javascript;
  udf_config_native.handler_name = "my_handler";
  udf_config_native.logical_commit_time = 5;
  DataRecordT data_record_native;
  data_record_native.record.Set(std::move(udf_config_native));
  auto [fbs_buffer, serialized_string_view] = Serialize(data_record_native);

  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call).Times(0);
  auto status = DeserializeRecord(serialized_string_view,
                                  record_callback.AsStdFunction());
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.message(), "code_snippet not set.");
}

TEST(DataRecordTest,
     DeserializeRecord_ToFbsRecord_UdfConfig_HandlerName_NotSet_Failure) {
  // Serialize
  UserDefinedFunctionsConfigT udf_config_native;
  udf_config_native.language = UserDefinedFunctionsLanguage::Javascript;
  udf_config_native.code_snippet = "function my_handler(){}";
  udf_config_native.logical_commit_time = 5;
  DataRecordT data_record_native;
  data_record_native.record.Set(std::move(udf_config_native));
  auto [fbs_buffer, serialized_string_view] = Serialize(data_record_native);

  testing::MockFunction<absl::Status(const DataRecord&)> record_callback;
  EXPECT_CALL(record_callback, Call).Times(0);
  auto status = DeserializeRecord(serialized_string_view,
                                  record_callback.AsStdFunction());
  EXPECT_FALSE(status.ok()) << status;
  EXPECT_EQ(status.message(), "handler_name not set.");
}

}  // namespace
}  // namespace kv_server
