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

#include "public/data_loading/csv/csv_delta_record_stream_writer.h"

#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "public/data_loading/data_loading_generated.h"

namespace kv_server {
namespace {

struct ValueStruct {
  std::string value_type;
  std::string value;
};

absl::StatusOr<std::string_view> GetMutationType(
    const KeyValueMutationRecordT& kv_record) {
  switch (kv_record.mutation_type) {
    case KeyValueMutationType::Update:
      return kUpdateMutationType;
    case KeyValueMutationType::Delete:
      return kDeleteMutationType;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid mutation type: ",
                       EnumNameKeyValueMutationType(kv_record.mutation_type)));
  }
}

std::string MaybeEncode(std::string_view value,
                        const CsvEncoding& csv_encoding) {
  if (csv_encoding == CsvEncoding::kBase64) {
    return absl::Base64Escape(value);
  }
  return std::string(value);
}

template <typename ElementType>
std::vector<std::string> MaybeEncode(std::vector<ElementType> values,
                                     const CsvEncoding& csv_encoding) {
  std::vector<std::string> result;
  if (csv_encoding == CsvEncoding::kBase64) {
    for (auto&& value : values) {
      result.push_back(absl::Base64Escape(absl::StrCat(value)));
    }
    return result;
  }
  std::transform(values.begin(), values.end(), std::back_inserter(result),
                 [](const auto elem) { return absl::StrCat(elem); });
  return result;
}

absl::StatusOr<ValueStruct> GetRecordValue(
    const KeyValueMutationRecordT& kv_record, std::string_view value_separator,
    const CsvEncoding& csv_encoding) {
  switch (kv_record.value.type) {
    case Value::StringValue:
      if (const auto* string_value = kv_record.value.AsStringValue();
          string_value != nullptr) {
        return ValueStruct{
            .value_type = std::string(kValueTypeString),
            .value = MaybeEncode(string_value->value, csv_encoding),
        };
      }
      return absl::InvalidArgumentError(
          "KeyValueMutationRecord string value is null");

    case Value::StringSet:
      if (const auto* string_set = kv_record.value.AsStringSet();
          string_set != nullptr) {
        return ValueStruct{
            .value_type = std::string(kValueTypeStringSet),
            .value = absl::StrJoin(MaybeEncode(string_set->value, csv_encoding),
                                   value_separator),
        };
      }
      return absl::InvalidArgumentError(
          "KeyValueMutationRecord string set value is null");

    case Value::UInt32Set:
      if (const auto* uint32_set = kv_record.value.AsUInt32Set();
          uint32_set != nullptr) {
        return ValueStruct{
            .value_type = std::string(kValueTypeUInt32Set),
            .value = absl::StrJoin(MaybeEncode(uint32_set->value, csv_encoding),
                                   value_separator),
        };
      }
      return absl::InvalidArgumentError(
          "KeyValueMutationRecord uint32 set value is null");

    case Value::UInt64Set:
      if (const auto* uint64_set = kv_record.value.AsUInt64Set();
          uint64_set != nullptr) {
        return ValueStruct{
            .value_type = std::string(kValueTypeUInt64Set),
            .value = absl::StrJoin(MaybeEncode(uint64_set->value, csv_encoding),
                                   value_separator),
        };
      }
      return absl::InvalidArgumentError(
          "KeyValueMutationRecord uint64 set value is null");

    default:
      return absl::InvalidArgumentError("KeyValueMutation value type unknown");
  }
}

absl::StatusOr<riegeli::CsvRecord> MakeCsvRecordWithKVMutation(
    const DataRecordT& data_record, const CsvEncoding& csv_encoding,
    char value_separator) {
  if (data_record.record.type != Record::KeyValueMutationRecord) {
    return absl::InvalidArgumentError(
        "DataRecord must contain a KeyValueMutationRecord.");
  }
  const auto* kv_record = data_record.record.AsKeyValueMutationRecord();
  if (kv_record == nullptr) {
    return absl::InvalidArgumentError("KeyValueMutationRecord is nullptr.");
  }

  riegeli::CsvRecord csv_record(*kKeyValueMutationRecordHeader);
  csv_record[kKeyColumn] = kv_record->key;
  absl::StatusOr<ValueStruct> value =
      GetRecordValue(*kv_record, std::string(1, value_separator), csv_encoding);
  if (!value.ok()) {
    return value.status();
  }
  csv_record[kValueColumn] = value->value;
  csv_record[kValueTypeColumn] = value->value_type;
  absl::StatusOr<std::string_view> mutation_type = GetMutationType(*kv_record);
  if (!mutation_type.ok()) {
    return mutation_type.status();
  }
  csv_record[kMutationTypeColumn] = *mutation_type;
  csv_record[kLogicalCommitTimeColumn] =
      absl::StrCat(kv_record->logical_commit_time);
  return csv_record;
}

absl::StatusOr<std::string_view> GetUdfLanguage(
    const UserDefinedFunctionsConfigT& udf_config) {
  switch (udf_config.language) {
    case UserDefinedFunctionsLanguage::Javascript:
      return kLanguageJavascript;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid UDF language: ",
          EnumNameUserDefinedFunctionsLanguage(udf_config.language)));
  }
}

absl::StatusOr<riegeli::CsvRecord> MakeCsvRecordWithUdfConfig(
    const DataRecordT& data_record) {
  if (data_record.record.type != Record::UserDefinedFunctionsConfig) {
    return absl::InvalidArgumentError(
        "DataRecord must contain a UserDefinedFunctionsConfig.");
  }

  const auto* udf_config = data_record.record.AsUserDefinedFunctionsConfig();
  if (udf_config == nullptr) {
    return absl::InvalidArgumentError("UserDefinedFunctionsConfig is nullptr.");
  }

  riegeli::CsvRecord csv_record(*kUserDefinedFunctionsConfigHeader);

  csv_record[kCodeSnippetColumn] = udf_config->code_snippet;
  csv_record[kHandlerNameColumn] = udf_config->handler_name;
  csv_record[kLogicalCommitTimeColumn] =
      absl::StrCat(udf_config->logical_commit_time);
  csv_record[kVersionColumn] = absl::StrCat(udf_config->version);

  auto udf_language = GetUdfLanguage(*udf_config);
  if (!udf_language.ok()) {
    return udf_language.status();
  }
  csv_record[kLanguageColumn] = *udf_language;
  return csv_record;
}

absl::StatusOr<riegeli::CsvRecord> MakeCsvRecordWithShardMapping(
    const DataRecordT& data_record) {
  if (data_record.record.type != Record::ShardMappingRecord) {
    return absl::InvalidArgumentError(
        "DataRecord must contain a ShardMappingRecord.");
  }

  const auto* shard_mapping_record = data_record.record.AsShardMappingRecord();
  if (shard_mapping_record == nullptr) {
    return absl::InvalidArgumentError("ShardMappingRecord is nullptr.");
  }

  riegeli::CsvRecord csv_record(*kShardMappingRecordHeader);
  csv_record[kLogicalShardColumn] =
      absl::StrCat(shard_mapping_record->logical_shard);
  csv_record[kPhysicalShardColumn] =
      absl::StrCat(shard_mapping_record->physical_shard);
  return csv_record;
}

}  // namespace

namespace internal {
absl::StatusOr<riegeli::CsvRecord> MakeCsvRecord(
    const DataRecordT& data_record, const DataRecordType& record_type,
    const CsvEncoding& csv_encoding, char value_separator) {
  // TODO: Consider using ctor with fields for performance gain if order is
  // known ahead of time.
  switch (record_type) {
    case DataRecordType::kKeyValueMutationRecord:
      return MakeCsvRecordWithKVMutation(data_record, csv_encoding,
                                         value_separator);
    case DataRecordType::kUserDefinedFunctionsConfig:
      return MakeCsvRecordWithUdfConfig(data_record);
    case DataRecordType::kShardMappingRecord:
      return MakeCsvRecordWithShardMapping(data_record);
    default:
      return absl::InvalidArgumentError("Invalid record type.");
  }
}
}  // namespace internal

}  // namespace kv_server
