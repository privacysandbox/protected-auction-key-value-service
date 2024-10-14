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
    const KeyValueMutationRecordStruct& record) {
  switch (record.mutation_type) {
    case KeyValueMutationType::Update:
      return kUpdateMutationType;
    case KeyValueMutationType::Delete:
      return kDeleteMutationType;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid mutation type: ",
                       EnumNameKeyValueMutationType(record.mutation_type)));
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
    const KeyValueMutationRecordValueT& value, std::string_view value_separator,
    const CsvEncoding& csv_encoding) {
  return std::visit(
      [value_separator,
       &csv_encoding](auto&& arg) -> absl::StatusOr<ValueStruct> {
        using VariantT = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<VariantT, std::string_view>) {
          return ValueStruct{
              .value_type = std::string(kValueTypeString),
              .value = MaybeEncode(arg, csv_encoding),
          };
        }
        if constexpr (std::is_same_v<VariantT, std::vector<std::string_view>>) {
          return ValueStruct{
              .value_type = std::string(kValueTypeStringSet),
              .value = absl::StrJoin(MaybeEncode(arg, csv_encoding),
                                     value_separator),
          };
        }
        if constexpr (std::is_same_v<VariantT, std::vector<uint32_t>>) {
          return ValueStruct{
              .value_type = std::string(kValueTypeUInt32Set),
              .value = absl::StrJoin(MaybeEncode(arg, csv_encoding),
                                     value_separator),
          };
        }
        if constexpr (std::is_same_v<VariantT, std::vector<uint64_t>>) {
          return ValueStruct{
              .value_type = std::string(kValueTypeUInt64Set),
              .value = absl::StrJoin(MaybeEncode(arg, csv_encoding),
                                     value_separator),
          };
        }
        return absl::InvalidArgumentError("Value must be set.");
      },
      value);
}

absl::StatusOr<riegeli::CsvRecord> MakeCsvRecordWithKVMutation(
    const DataRecordStruct& data_record, const CsvEncoding& csv_encoding,
    char value_separator) {
  if (!std::holds_alternative<KeyValueMutationRecordStruct>(
          data_record.record)) {
    return absl::InvalidArgumentError(
        "DataRecord must contain a KeyValueMutationRecord.");
  }
  const auto record =
      std::get<KeyValueMutationRecordStruct>(data_record.record);

  riegeli::CsvRecord csv_record(*kKeyValueMutationRecordHeader);
  csv_record[kKeyColumn] = record.key;
  absl::StatusOr<ValueStruct> value = GetRecordValue(
      record.value, std::string(1, value_separator), csv_encoding);
  if (!value.ok()) {
    return value.status();
  }
  csv_record[kValueColumn] = value->value;
  csv_record[kValueTypeColumn] = value->value_type;
  absl::StatusOr<std::string_view> mutation_type = GetMutationType(record);
  if (!mutation_type.ok()) {
    return mutation_type.status();
  }
  csv_record[kMutationTypeColumn] = *mutation_type;
  csv_record[kLogicalCommitTimeColumn] =
      absl::StrCat(record.logical_commit_time);
  return csv_record;
}

absl::StatusOr<std::string_view> GetUdfLanguage(
    const UserDefinedFunctionsConfigStruct& udf_config) {
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
    const DataRecordStruct& data_record) {
  if (!std::holds_alternative<UserDefinedFunctionsConfigStruct>(
          data_record.record)) {
    return absl::InvalidArgumentError(
        "DataRecord must contain a UserDefinedFunctionsConfig.");
  }
  const auto udf_config =
      std::get<UserDefinedFunctionsConfigStruct>(data_record.record);

  riegeli::CsvRecord csv_record(*kUserDefinedFunctionsConfigHeader);

  csv_record[kCodeSnippetColumn] = udf_config.code_snippet;
  csv_record[kHandlerNameColumn] = udf_config.handler_name;
  csv_record[kLogicalCommitTimeColumn] =
      absl::StrCat(udf_config.logical_commit_time);
  csv_record[kVersionColumn] = absl::StrCat(udf_config.version);

  auto udf_language = GetUdfLanguage(udf_config);
  if (!udf_language.ok()) {
    return udf_language.status();
  }
  csv_record[kLanguageColumn] = *udf_language;
  return csv_record;
}

absl::StatusOr<riegeli::CsvRecord> MakeCsvRecordWithShardMapping(
    const DataRecordStruct& data_record) {
  if (!std::holds_alternative<ShardMappingRecordStruct>(data_record.record)) {
    return absl::InvalidArgumentError(
        "DataRecord must contain a ShardMappingRecord.");
  }
  const auto shard_mapping_struct =
      std::get<ShardMappingRecordStruct>(data_record.record);
  riegeli::CsvRecord csv_record(*kShardMappingRecordHeader);
  csv_record[kLogicalShardColumn] =
      absl::StrCat(shard_mapping_struct.logical_shard);
  csv_record[kPhysicalShardColumn] =
      absl::StrCat(shard_mapping_struct.physical_shard);
  return csv_record;
}

}  // namespace

namespace internal {
absl::StatusOr<riegeli::CsvRecord> MakeCsvRecord(
    const DataRecordStruct& data_record, const DataRecordType& record_type,
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
