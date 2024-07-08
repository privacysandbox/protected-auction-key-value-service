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

#include "public/data_loading/records_utils.h"

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "flatbuffers/flatbuffer_builder.h"
#include "public/data_loading/record_utils.h"

namespace kv_server {
namespace {

struct ValueUnion {
  Value value_type;
  flatbuffers::Offset<void> value;
};

struct RecordUnion {
  Record record_type;
  flatbuffers::Offset<void> record;
};

ValueUnion BuildValueUnion(const KeyValueMutationRecordValueT& value,
                           flatbuffers::FlatBufferBuilder& builder) {
  return std::visit(
      [&builder](auto&& arg) {
        using VariantT = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<VariantT, std::string_view>) {
          return ValueUnion{
              .value_type = Value::StringValue,
              .value = CreateStringValueDirect(builder, arg.data()).Union(),
          };
        }
        if constexpr (std::is_same_v<VariantT, std::vector<std::string_view>>) {
          auto values_offset = builder.CreateVectorOfStrings(arg);
          return ValueUnion{
              .value_type = Value::StringSet,
              .value = CreateStringSet(builder, values_offset).Union(),
          };
        }
        if constexpr (std::is_same_v<VariantT, std::vector<uint32_t>>) {
          auto values_offset = builder.CreateVector(arg);
          return ValueUnion{
              .value_type = Value::UInt32Set,
              .value = CreateUInt32Set(builder, values_offset).Union(),
          };
        }
        if constexpr (std::is_same_v<VariantT, std::monostate>) {
          return ValueUnion{
              .value_type = Value::NONE,
              .value = flatbuffers::Offset<void>(),
          };
        }
      },
      value);
}

flatbuffers::Offset<KeyValueMutationRecord> KeyValueMutationFromStruct(
    flatbuffers::FlatBufferBuilder& builder,
    const KeyValueMutationRecordStruct& record) {
  auto fb_value = BuildValueUnion(record.value, builder);
  return CreateKeyValueMutationRecordDirect(
      builder, record.mutation_type, record.logical_commit_time,
      record.key.data(), fb_value.value_type, fb_value.value);
}

flatbuffers::Offset<UserDefinedFunctionsConfig> UdfConfigFromStruct(
    flatbuffers::FlatBufferBuilder& builder,
    const UserDefinedFunctionsConfigStruct& udf_config_struct) {
  return CreateUserDefinedFunctionsConfigDirect(
      builder, udf_config_struct.language,
      udf_config_struct.code_snippet.data(),
      udf_config_struct.handler_name.data(),
      udf_config_struct.logical_commit_time, udf_config_struct.version);
}

flatbuffers::Offset<ShardMappingRecord> ShardMappingFromStruct(
    flatbuffers::FlatBufferBuilder& builder,
    const ShardMappingRecordStruct& shard_mapping_struct) {
  return CreateShardMappingRecord(builder, shard_mapping_struct.logical_shard,
                                  shard_mapping_struct.physical_shard);
}

RecordUnion BuildRecordUnion(const RecordT& record,
                             flatbuffers::FlatBufferBuilder& builder) {
  return std::visit(
      [&builder](auto&& arg) {
        using VariantT = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<VariantT, KeyValueMutationRecordStruct>) {
          return RecordUnion{
              .record_type = Record::KeyValueMutationRecord,
              .record = KeyValueMutationFromStruct(builder, arg).Union(),
          };
        }
        if constexpr (std::is_same_v<VariantT,
                                     UserDefinedFunctionsConfigStruct>) {
          return RecordUnion{
              .record_type = Record::UserDefinedFunctionsConfig,
              .record = UdfConfigFromStruct(builder, arg).Union(),
          };
        }
        if constexpr (std::is_same_v<VariantT, ShardMappingRecordStruct>) {
          return RecordUnion{
              .record_type = Record::ShardMappingRecord,
              .record = ShardMappingFromStruct(builder, arg).Union(),
          };
        }
        if constexpr (std::is_same_v<VariantT, std::monostate>) {
          return RecordUnion{
              .record_type = Record::NONE,
              .record = flatbuffers::Offset<void>(),
          };
        }
      },
      record);
}

template <typename FbsRecordT>
absl::StatusOr<const FbsRecordT*> DeserializeAndVerifyRecord(
    std::string_view record_bytes) {
  auto fbs_record = flatbuffers::GetRoot<FbsRecordT>(record_bytes.data());
  auto record_verifier = flatbuffers::Verifier(
      reinterpret_cast<const uint8_t*>(record_bytes.data()),
      record_bytes.size(), flatbuffers::Verifier::Options{});
  if (!fbs_record->Verify(record_verifier)) {
    // TODO(b/239061954): Publish metrics for alerting
    return absl::InvalidArgumentError("Invalid flatbuffer bytes.");
  }
  return fbs_record;
}

absl::Status ValidateValue(const KeyValueMutationRecord& kv_mutation_record) {
  if (kv_mutation_record.value() == nullptr) {
    return absl::InvalidArgumentError("Value not set.");
  }
  if (kv_mutation_record.value_type() == Value::StringValue &&
      (kv_mutation_record.value_as_StringValue() == nullptr ||
       kv_mutation_record.value_as_StringValue()->value() == nullptr)) {
    return absl::InvalidArgumentError("String value not set.");
  }
  if (kv_mutation_record.value_type() == Value::StringSet &&
      (kv_mutation_record.value_as_StringSet() == nullptr ||
       kv_mutation_record.value_as_StringSet()->value() == nullptr)) {
    return absl::InvalidArgumentError("StringSet value not set.");
  }
  if (kv_mutation_record.value_type() == Value::UInt32Set &&
      (kv_mutation_record.value_as_UInt32Set() == nullptr ||
       kv_mutation_record.value_as_UInt32Set()->value() == nullptr)) {
    return absl::InvalidArgumentError("UInt32Set value not set.");
  }
  return absl::OkStatus();
}

absl::Status ValidateKeyValueMutationRecord(
    const KeyValueMutationRecord& kv_mutation_record) {
  if (kv_mutation_record.key() == nullptr) {
    return absl::InvalidArgumentError("Key not set.");
  }
  return ValidateValue(kv_mutation_record);
}

absl::Status ValidateUserDefinedFunctionsConfig(
    const UserDefinedFunctionsConfig& udf_config) {
  if (udf_config.code_snippet() == nullptr) {
    return absl::InvalidArgumentError("code_snippet not set.");
  }
  if (udf_config.handler_name() == nullptr) {
    return absl::InvalidArgumentError("handler_name not set.");
  }
  return absl::OkStatus();
}

absl::Status ValidateData(const DataRecord& data_record) {
  if (data_record.record() == nullptr) {
    return absl::InvalidArgumentError("Record not set.");
  }

  if (data_record.record_type() == Record::KeyValueMutationRecord) {
    if (const auto status = ValidateKeyValueMutationRecord(
            *data_record.record_as_KeyValueMutationRecord());
        !status.ok()) {
      return status;
    }
  }

  if (data_record.record_type() == Record::UserDefinedFunctionsConfig) {
    if (const auto status = ValidateUserDefinedFunctionsConfig(
            *data_record.record_as_UserDefinedFunctionsConfig());
        !status.ok()) {
      return status;
    }
  }
  return absl::OkStatus();
}

KeyValueMutationRecordValueT GetRecordStructValue(
    const KeyValueMutationRecord& fbs_record) {
  KeyValueMutationRecordValueT value;
  if (fbs_record.value_type() == Value::StringValue) {
    value = GetRecordValue<std::string_view>(fbs_record);
  }
  if (fbs_record.value_type() == Value::StringSet) {
    value = GetRecordValue<std::vector<std::string_view>>(fbs_record);
  }
  if (fbs_record.value_type() == Value::UInt32Set) {
    value = GetRecordValue<std::vector<uint32_t>>(fbs_record);
  }
  return value;
}

RecordT GetRecordStruct(const DataRecord& data_record) {
  RecordT record;
  if (data_record.record_type() == Record::KeyValueMutationRecord) {
    record = GetTypedRecordStruct<KeyValueMutationRecordStruct>(data_record);
  }
  if (data_record.record_type() == Record::UserDefinedFunctionsConfig) {
    record =
        GetTypedRecordStruct<UserDefinedFunctionsConfigStruct>(data_record);
  }
  if (data_record.record_type() == Record::ShardMappingRecord) {
    record = GetTypedRecordStruct<ShardMappingRecordStruct>(data_record);
  }
  return record;
}

}  // namespace

bool operator==(const KeyValueMutationRecordStruct& lhs_record,
                const KeyValueMutationRecordStruct& rhs_record) {
  return lhs_record.logical_commit_time == rhs_record.logical_commit_time &&
         lhs_record.mutation_type == rhs_record.mutation_type &&
         lhs_record.key == rhs_record.key &&
         lhs_record.value == rhs_record.value;
}

bool operator!=(const KeyValueMutationRecordStruct& lhs_record,
                const KeyValueMutationRecordStruct& rhs_record) {
  return !operator==(lhs_record, rhs_record);
}

bool operator==(const UserDefinedFunctionsConfigStruct& lhs_record,
                const UserDefinedFunctionsConfigStruct& rhs_record) {
  return lhs_record.logical_commit_time == rhs_record.logical_commit_time &&
         lhs_record.version == rhs_record.version &&
         lhs_record.handler_name == rhs_record.handler_name &&
         lhs_record.language == rhs_record.language &&
         lhs_record.code_snippet == rhs_record.code_snippet;
}

bool operator!=(const UserDefinedFunctionsConfigStruct& lhs_record,
                const UserDefinedFunctionsConfigStruct& rhs_record) {
  return !operator==(lhs_record, rhs_record);
}

bool operator==(const ShardMappingRecordStruct& lhs_record,
                const ShardMappingRecordStruct& rhs_record) {
  return lhs_record.logical_shard == rhs_record.logical_shard &&
         lhs_record.physical_shard == rhs_record.physical_shard;
}

bool operator!=(const ShardMappingRecordStruct& lhs_record,
                const ShardMappingRecordStruct& rhs_record) {
  return !operator==(lhs_record, rhs_record);
}

bool operator==(const DataRecordStruct& lhs_record,
                const DataRecordStruct& rhs_record) {
  return lhs_record.record == rhs_record.record;
}

bool operator!=(const DataRecordStruct& lhs_record,
                const DataRecordStruct& rhs_record) {
  return !operator==(lhs_record, rhs_record);
}

bool IsEmptyValue(const KeyValueMutationRecordValueT& value) {
  return value.index() == 0;
}

flatbuffers::FlatBufferBuilder ToFlatBufferBuilder(
    const KeyValueMutationRecordStruct& record) {
  flatbuffers::FlatBufferBuilder builder;
  const auto fbs_record = KeyValueMutationFromStruct(builder, record);
  builder.Finish(fbs_record);
  return builder;
}

flatbuffers::FlatBufferBuilder ToFlatBufferBuilder(
    const DataRecordStruct& data_record) {
  flatbuffers::FlatBufferBuilder builder;
  auto kv_fbs_record = BuildRecordUnion(data_record.record, builder);
  const auto fbs_record = CreateDataRecord(builder, kv_fbs_record.record_type,
                                           kv_fbs_record.record);
  builder.Finish(fbs_record);
  return builder;
}

absl::Status DeserializeRecord(
    std::string_view record_bytes,
    const std::function<absl::Status(const KeyValueMutationRecordStruct&)>&
        record_callback) {
  return DeserializeRecord(
      record_bytes,
      [&record_callback](const KeyValueMutationRecord& fbs_record) {
        KeyValueMutationRecordStruct record_struct;
        record_struct.key = fbs_record.key()->string_view();
        record_struct.logical_commit_time = fbs_record.logical_commit_time();
        record_struct.mutation_type = fbs_record.mutation_type();
        record_struct.value = GetRecordStructValue(fbs_record);
        return record_callback(record_struct);
      });
}

absl::Status DeserializeDataRecord(
    std::string_view record_bytes,
    const std::function<absl::Status(const DataRecord&)>& record_callback) {
  auto fbs_record = DeserializeAndVerifyRecord<DataRecord>(record_bytes);
  if (!fbs_record.ok()) {
    LOG_FIRST_N(ERROR, 3) << "Record deserialization failed: "
                          << fbs_record.status();
    return fbs_record.status();
  }
  if (const auto status = ValidateData(**fbs_record); !status.ok()) {
    LOG_FIRST_N(ERROR, 3) << "Data validation failed: " << status;
    return status;
  }
  return record_callback(**fbs_record);
}

absl::Status DeserializeDataRecord(
    std::string_view record_bytes,
    const std::function<absl::Status(const DataRecordStruct&)>&
        record_callback) {
  return DeserializeDataRecord(
      record_bytes, [&record_callback](const DataRecord& fbs_record) {
        DataRecordStruct data_struct;
        data_struct.record = GetRecordStruct(fbs_record);
        return record_callback(data_struct);
      });
}

template <>
std::string_view GetRecordValue(const KeyValueMutationRecord& record) {
  return record.value_as_StringValue()->value()->string_view();
}

template <>
std::vector<std::string_view> GetRecordValue(
    const KeyValueMutationRecord& record) {
  std::vector<std::string_view> values;
  for (const auto val : *record.value_as_StringSet()->value()) {
    values.push_back(val->string_view());
  }
  return values;
}

template <>
std::vector<uint32_t> GetRecordValue(const KeyValueMutationRecord& record) {
  return std::vector<uint32_t>(record.value_as_UInt32Set()->value()->begin(),
                               record.value_as_UInt32Set()->value()->end());
}

template <>
KeyValueMutationRecordStruct GetTypedRecordStruct(
    const DataRecord& data_record) {
  KeyValueMutationRecordStruct kv_mutation_struct;
  const auto* kv_mutation_record =
      data_record.record_as_KeyValueMutationRecord();
  kv_mutation_struct.key = kv_mutation_record->key()->string_view();
  kv_mutation_struct.logical_commit_time =
      kv_mutation_record->logical_commit_time();
  kv_mutation_struct.mutation_type = kv_mutation_record->mutation_type();
  kv_mutation_struct.value = GetRecordStructValue(*kv_mutation_record);
  return kv_mutation_struct;
}

template <>
UserDefinedFunctionsConfigStruct GetTypedRecordStruct(
    const DataRecord& data_record) {
  UserDefinedFunctionsConfigStruct udf_config_struct;
  const auto* udf_config = data_record.record_as_UserDefinedFunctionsConfig();
  udf_config_struct.language = udf_config->language();
  udf_config_struct.logical_commit_time = udf_config->logical_commit_time();
  udf_config_struct.code_snippet = udf_config->code_snippet()->string_view();
  udf_config_struct.handler_name = udf_config->handler_name()->string_view();
  udf_config_struct.version = udf_config->version();
  return udf_config_struct;
}

template <>
ShardMappingRecordStruct GetTypedRecordStruct(const DataRecord& data_record) {
  const auto* shard_mapping_record = data_record.record_as_ShardMappingRecord();
  return ShardMappingRecordStruct{
      .logical_shard = shard_mapping_record->logical_shard(),
      .physical_shard = shard_mapping_record->physical_shard(),
  };
}

}  // namespace kv_server
