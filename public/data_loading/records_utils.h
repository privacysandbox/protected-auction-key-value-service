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

#ifndef PUBLIC_DATA_LOADING_RECORDS_UTILS_H_
#define PUBLIC_DATA_LOADING_RECORDS_UTILS_H_

#include <string_view>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/record_utils.h"

namespace kv_server {

enum class DataRecordType : int {
  kKeyValueMutationRecord,
  kUserDefinedFunctionsConfig,
  kShardMappingRecord
};

using KeyValueMutationRecordValueT =
    std::variant<std::monostate, std::string_view,
                 std::vector<std::string_view>, std::vector<uint32_t>>;

struct KeyValueMutationRecordStruct {
  KeyValueMutationType mutation_type;
  int64_t logical_commit_time;
  std::string_view key;
  KeyValueMutationRecordValueT value;
};

struct UserDefinedFunctionsConfigStruct {
  UserDefinedFunctionsLanguage language;
  std::string_view code_snippet;
  std::string_view handler_name;
  int64_t logical_commit_time;
  int64_t version;
};

struct ShardMappingRecordStruct {
  int32_t logical_shard;
  int32_t physical_shard;
};

using RecordT =
    std::variant<std::monostate, KeyValueMutationRecordStruct,
                 UserDefinedFunctionsConfigStruct, ShardMappingRecordStruct>;

struct DataRecordStruct {
  RecordT record;
};

bool operator==(const KeyValueMutationRecordStruct& lhs_record,
                const KeyValueMutationRecordStruct& rhs_record);
bool operator!=(const KeyValueMutationRecordStruct& lhs_record,
                const KeyValueMutationRecordStruct& rhs_record);

bool operator==(const UserDefinedFunctionsConfigStruct& lhs_record,
                const UserDefinedFunctionsConfigStruct& rhs_record);
bool operator!=(const UserDefinedFunctionsConfigStruct& lhs_record,
                const UserDefinedFunctionsConfigStruct& rhs_record);

bool operator==(const ShardMappingRecordStruct& lhs_record,
                const ShardMappingRecordStruct& rhs_record);
bool operator!=(const ShardMappingRecordStruct& lhs_record,
                const ShardMappingRecordStruct& rhs_record);

bool operator==(const DataRecordStruct& lhs_record,
                const DataRecordStruct& rhs_record);
bool operator!=(const DataRecordStruct& lhs_record,
                const DataRecordStruct& rhs_record);

// Returns true if the value has been default initialized and no variant was
// set.
bool IsEmptyValue(const KeyValueMutationRecordValueT& value);

// Serializes the record struct to a flat buffer builder using format defined by
// `data_loading.fbs:KeyValueMutationRecord` table.
flatbuffers::FlatBufferBuilder ToFlatBufferBuilder(
    const KeyValueMutationRecordStruct& record);

// Serializes the file record struct to a flat buffer builder using format
// defined by `data_loading.fbs:DataRecord` table.
flatbuffers::FlatBufferBuilder ToFlatBufferBuilder(
    const DataRecordStruct& data_record);

// Deserializes "data_loading.fbs:KeyValueMutationRecord" raw flatbuffer record
// bytes and calls `record_callback` with the resulting
// `KeyValueMutationRecordStruct` object.
// Returns `absl::InvalidArgumentError` if deserilization fails, otherwise
// returns the result of calling `record_callback`.
absl::Status DeserializeRecord(
    std::string_view record_bytes,
    const std::function<absl::Status(const KeyValueMutationRecordStruct&)>&
        record_callback);

// Deserializes "data_loading.fbs:DataRecord" raw flatbuffer record
// bytes and calls `record_callback` with the resulting `DataRecord`
// object.
// Returns `absl::InvalidArgumentError` if deserialization fails, otherwise
// returns the result of calling `record_callback`.
absl::Status DeserializeDataRecord(
    std::string_view record_bytes,
    const std::function<absl::Status(const DataRecord&)>& record_callback);

// Deserializes "data_loading.fbs:DataRecord" raw flatbuffer record
// bytes and calls `record_callback` with the resulting
// `DataRecordStruct` object.
// Returns `absl::InvalidArgumentError` if deserilization fails, otherwise
// returns the result of calling `record_callback`.
absl::Status DeserializeDataRecord(
    std::string_view record_bytes,
    const std::function<absl::Status(const DataRecordStruct&)>&
        record_callback);

// Utility function to get the union value set on the `record`. Must
// be called after checking the type of the union value using
// `record.value_type()` function.
template <typename ValueT>
ValueT GetRecordValue(const KeyValueMutationRecord& record);
template <>
std::string_view GetRecordValue(const KeyValueMutationRecord& record);
template <>
std::vector<std::string_view> GetRecordValue(
    const KeyValueMutationRecord& record);
template <>
std::vector<uint32_t> GetRecordValue(const KeyValueMutationRecord& record);

// Utility function to get the union record set on the `data_record`. Must
// be called after checking the type of the union record using
// `data_record.record_type()` function.
template <typename RecordT>
RecordT GetTypedRecordStruct(const DataRecord& data_record);
template <>
KeyValueMutationRecordStruct GetTypedRecordStruct(
    const DataRecord& data_record);
template <>
UserDefinedFunctionsConfigStruct GetTypedRecordStruct(
    const DataRecord& data_record);
template <>
ShardMappingRecordStruct GetTypedRecordStruct(const DataRecord& data_record);

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_RECORDS_UTILS_H_
