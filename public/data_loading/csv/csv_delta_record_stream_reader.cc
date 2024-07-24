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

#include "public/data_loading/csv/csv_delta_record_stream_reader.h"

#include <utility>

#include "absl/log/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "public/data_loading/record_utils.h"
#include "src/util/status_macro/status_macros.h"

namespace kv_server {
namespace {

absl::StatusOr<int64_t> GetInt64Column(const riegeli::CsvRecord& csv_record,
                                       std::string_view column_name) {
  if (int64_t result; absl::SimpleAtoi(csv_record[column_name], &result)) {
    return result;
  }
  return absl::InvalidArgumentError(absl::StrCat("Cannot convert ", column_name,
                                                 ":", csv_record[column_name],
                                                 " to a number."));
}

absl::StatusOr<std::string> GetValue(const riegeli::CsvRecord& csv_record,
                                     char value_separator,
                                     const CsvEncoding& csv_encoding) {
  if (csv_encoding == CsvEncoding::kBase64) {
    std::string result;
    if (absl::Base64Unescape(csv_record[kValueColumn], &result)) {
      return result;
    }
    return absl::InvalidArgumentError(absl::StrCat(
        "base64 decode failed for value: ", csv_record[kValueColumn]));
  }
  return csv_record[kValueColumn];
}

template <typename ElementType>
absl::StatusOr<std::vector<ElementType>> BuildSetValue(
    const std::vector<std::string>& set) {
  std::vector<ElementType> result;
  for (auto&& set_value : set) {
    if constexpr (std::is_same_v<ElementType, std::string>) {
      result.push_back(std::string(set_value));
    }
    if constexpr (std::is_same_v<ElementType, uint32_t> ||
                  std::is_same_v<ElementType, uint64_t>) {
      if (ElementType number; absl::SimpleAtoi(set_value, &number)) {
        result.push_back(number);
      } else {
        return absl::InvalidArgumentError(
            absl::StrCat("Cannot convert: ", set_value, " to a number."));
      }
    }
  }
  return result;
}

template <typename ElementType>
absl::StatusOr<std::vector<ElementType>> GetSetValue(
    const riegeli::CsvRecord& csv_record, char value_separator,
    const CsvEncoding& csv_encoding) {
  if (csv_encoding == CsvEncoding::kBase64) {
    std::vector<std::string> decoded_values;
    for (auto&& set_value :
         absl::StrSplit(csv_record[kValueColumn], value_separator)) {
      if (std::string dest; absl::Base64Unescape(set_value, &dest)) {
        decoded_values.push_back(std::move(dest));
      } else {
        return absl::InvalidArgumentError(absl::StrCat(
            "base64 decode failed for value: ", csv_record[kValueColumn]));
      }
    }
    return BuildSetValue<ElementType>(decoded_values);
  }
  return BuildSetValue<ElementType>(
      absl::StrSplit(csv_record[kValueColumn], value_separator));
}

absl::StatusOr<KeyValueMutationType> GetDeltaMutationType(
    absl::string_view mutation_type) {
  if (absl::EqualsIgnoreCase(
          mutation_type,
          EnumNameKeyValueMutationType(KeyValueMutationType::Update))) {
    return KeyValueMutationType::Update;
  }
  if (absl::EqualsIgnoreCase(
          mutation_type,
          EnumNameKeyValueMutationType(KeyValueMutationType::Delete))) {
    return KeyValueMutationType::Delete;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Unknown mutation type:", mutation_type));
}

absl::Status SetRecordValue(char value_separator,
                            const CsvEncoding& csv_encoding,
                            const riegeli::CsvRecord& csv_record,
                            KeyValueMutationRecordT& mutation_record) {
  auto type = csv_record[kValueTypeColumn];
  VLOG(10) << "Setting record value of type: " << type;
  if (absl::EqualsIgnoreCase(type, kValueTypeString)) {
    if (auto maybe_value = GetValue(csv_record, value_separator, csv_encoding);
        !maybe_value.ok()) {
      return maybe_value.status();
    } else {
      StringValueT string_native;
      string_native.value = *maybe_value;
      VLOG(9) << "value in CSV record is:" << string_native.value;
      mutation_record.value.Set(std::move(string_native));
      VLOG(9) << "Built C++ record value: " << mutation_record.value;
    }
    return absl::OkStatus();
  }
  if (absl::EqualsIgnoreCase(type, kValueTypeStringSet)) {
    auto maybe_value =
        GetSetValue<std::string>(csv_record, value_separator, csv_encoding);
    if (!maybe_value.ok()) {
      return maybe_value.status();
    }
    StringSetT set_value;
    set_value.value = std::move(*maybe_value);
    mutation_record.value.Set(std::move(set_value));
    return absl::OkStatus();
  }
  if (absl::EqualsIgnoreCase(type, kValueTypeUInt32Set)) {
    PS_ASSIGN_OR_RETURN(
        auto value,
        GetSetValue<uint32_t>(csv_record, value_separator, csv_encoding));
    UInt32SetT set_value;
    set_value.value = std::move(value);
    mutation_record.value.Set(std::move(set_value));
    return absl::OkStatus();
  }
  if (absl::EqualsIgnoreCase(type, kValueTypeUInt64Set)) {
    PS_ASSIGN_OR_RETURN(
        auto value,
        GetSetValue<uint64_t>(csv_record, value_separator, csv_encoding));
    UInt64SetT set_value;
    set_value.value = std::move(value);
    mutation_record.value.Set(std::move(set_value));
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Value type: ", type, " is not supported"));
}

absl::StatusOr<DataRecordT> MakeDeltaFileRecordStructWithKVMutation(
    char value_separator, const CsvEncoding& csv_encoding,
    const riegeli::CsvRecord& csv_record) {
  KeyValueMutationRecordT record;
  record.key = csv_record[kKeyColumn];
  if (auto status =
          SetRecordValue(value_separator, csv_encoding, csv_record, record);
      !status.ok()) {
    return status;
  }
  VLOG(10) << "Built C++ KeyValueMutationRecordT record: " << record;
  absl::StatusOr<int64_t> commit_time =
      GetInt64Column(csv_record, kLogicalCommitTimeColumn);
  if (!commit_time.ok()) {
    return commit_time.status();
  }
  record.logical_commit_time = *commit_time;
  absl::StatusOr<KeyValueMutationType> mutation_type =
      GetDeltaMutationType(csv_record[kMutationTypeColumn]);
  if (!mutation_type.ok()) {
    return mutation_type.status();
  }
  record.mutation_type = *mutation_type;

  DataRecordT data_record;
  data_record.record.Set(std::move(record));
  VLOG(9) << "Built C++ data record: " << data_record;
  return data_record;
}

absl::StatusOr<UserDefinedFunctionsLanguage> GetUdfLanguage(
    const riegeli::CsvRecord& csv_record) {
  auto language = absl::AsciiStrToLower(csv_record[kLanguageColumn]);
  if (kLanguageJavascript == language) {
    return UserDefinedFunctionsLanguage::Javascript;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Language: ", language, " is not supported."));
}

absl::StatusOr<DataRecordT> MakeDeltaFileRecordStructWithUdfConfig(
    const riegeli::CsvRecord& csv_record) {
  UserDefinedFunctionsConfigT udf_config;
  udf_config.code_snippet = csv_record[kCodeSnippetColumn];
  udf_config.handler_name = csv_record[kHandlerNameColumn];

  absl::StatusOr<int64_t> commit_time =
      GetInt64Column(csv_record, kLogicalCommitTimeColumn);
  if (!commit_time.ok()) {
    return commit_time.status();
  }
  udf_config.logical_commit_time = *commit_time;

  absl::StatusOr<int64_t> version = GetInt64Column(csv_record, kVersionColumn);
  if (!version.ok()) {
    return version.status();
  }
  udf_config.version = *version;

  auto language = GetUdfLanguage(csv_record);
  if (!language.ok()) {
    return language.status();
  }
  udf_config.language = *language;

  DataRecordT data_record;
  data_record.record.Set(std::move(udf_config));
  return data_record;
}

absl::StatusOr<DataRecordT> MakeDeltaFileRecordStructWithShardMapping(
    const riegeli::CsvRecord& csv_record) {
  ShardMappingRecordT shard_mapping_struct;
  absl::StatusOr<int64_t> logical_shard =
      GetInt64Column(csv_record, kLogicalShardColumn);
  if (!logical_shard.ok()) {
    return logical_shard.status();
  }
  shard_mapping_struct.logical_shard = *logical_shard;
  absl::StatusOr<int64_t> physical_shard =
      GetInt64Column(csv_record, kPhysicalShardColumn);
  if (!physical_shard.ok()) {
    return physical_shard.status();
  }
  shard_mapping_struct.physical_shard = *physical_shard;
  DataRecordT data_record;
  data_record.record.Set(std::move(shard_mapping_struct));
  return data_record;
}
}  // namespace

namespace internal {
absl::StatusOr<DataRecordT> MakeDeltaFileRecordStruct(
    const riegeli::CsvRecord& csv_record,
    const CsvDeltaRecordStreamReaderOptions& options) {
  switch (options.record_type) {
    case Record::KeyValueMutationRecord:
      return MakeDeltaFileRecordStructWithKVMutation(
          options.value_separator, options.csv_encoding, csv_record);
    case Record::UserDefinedFunctionsConfig:
      return MakeDeltaFileRecordStructWithUdfConfig(csv_record);
    case Record::ShardMappingRecord:
      return MakeDeltaFileRecordStructWithShardMapping(csv_record);
    default:
      return absl::InvalidArgumentError("Invalid record type.");
  }
}

}  // namespace internal
}  // namespace kv_server
