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

#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "glog/logging.h"
#include "public/data_loading/records_utils.h"
#include "src/cpp/util/status_macro/status_macros.h"

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

absl::StatusOr<KeyValueMutationType> GetDeltaMutationType(
    absl::string_view mutation_type) {
  std::string mt_lower = absl::AsciiStrToLower(mutation_type);
  if (mt_lower == kUpdateMutationType) {
    return KeyValueMutationType::Update;
  }
  if (mt_lower == kDeleteMutationType) {
    return KeyValueMutationType::Delete;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Unknown mutation type:", mutation_type));
}

absl::StatusOr<KeyValueMutationRecordValueT> GetRecordValue(
    const riegeli::CsvRecord& csv_record, std::string_view value,
    const std::vector<std::string>& set_value) {
  auto type = absl::AsciiStrToLower(csv_record[kValueTypeColumn]);
  if (kValueTypeString == type) {
    return value;
  }
  if (kValueTypeStringSet == type) {
    return std::vector<std::string_view>(set_value.begin(), set_value.end());
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Value type: ", type, " is not supported"));
}

absl::StatusOr<DataRecordStruct> MakeDeltaFileRecordStructWithKVMutation(
    const riegeli::CsvRecord& csv_record, std::string_view value,
    const std::vector<std::string>& set_value) {
  KeyValueMutationRecordStruct record;
  record.key = csv_record[kKeyColumn];
  PS_ASSIGN_OR_RETURN(record.value,
                      GetRecordValue(csv_record, value, set_value));
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

  DataRecordStruct data_record;
  data_record.record = record;
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

absl::StatusOr<DataRecordStruct> MakeDeltaFileRecordStructWithUdfConfig(
    const riegeli::CsvRecord& csv_record) {
  UserDefinedFunctionsConfigStruct udf_config;
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

  DataRecordStruct data_record;
  data_record.record = udf_config;
  return data_record;
}

absl::StatusOr<DataRecordStruct> MakeDeltaFileRecordStructWithShardMapping(
    const riegeli::CsvRecord& csv_record) {
  ShardMappingRecordStruct shard_mapping_struct;
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
  DataRecordStruct data_record;
  data_record.record = shard_mapping_struct;
  return data_record;
}
}  // namespace

namespace internal {
absl::StatusOr<std::string> MaybeGetValue(const riegeli::CsvRecord& csv_record,
                                          const CsvEncoding& csv_encoding) {
  auto type = absl::AsciiStrToLower(csv_record[kValueTypeColumn]);
  if (kValueTypeString != type) {
    return "";
  }
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

absl::StatusOr<std::vector<std::string>> MaybeGetSetValue(
    const riegeli::CsvRecord& csv_record, char value_separator,
    const CsvEncoding& csv_encoding) {
  auto type = absl::AsciiStrToLower(csv_record[kValueTypeColumn]);
  if (kValueTypeStringSet != type) {
    return std::vector<std::string>();
  }
  if (csv_encoding == CsvEncoding::kBase64) {
    std::vector<std::string> result;
    for (auto&& set_value :
         absl::StrSplit(csv_record[kValueColumn], value_separator)) {
      if (std::string dest; absl::Base64Unescape(set_value, &dest)) {
        result.push_back(std::move(dest));
      } else {
        return absl::InvalidArgumentError(absl::StrCat(
            "base64 decode failed for value: ", csv_record[kValueColumn]));
      }
    }
    return result;
  }
  return absl::StrSplit(csv_record[kValueColumn], value_separator);
}

absl::StatusOr<DataRecordStruct> MakeDeltaFileRecordStruct(
    const riegeli::CsvRecord& csv_record, const DataRecordType& record_type,
    std::string_view value, const std::vector<std::string>& set_value) {
  switch (record_type) {
    case DataRecordType::kKeyValueMutationRecord:
      return MakeDeltaFileRecordStructWithKVMutation(csv_record, value,
                                                     set_value);
    case DataRecordType::kUserDefinedFunctionsConfig:
      return MakeDeltaFileRecordStructWithUdfConfig(csv_record);
    case DataRecordType::kShardMappingRecord:
      return MakeDeltaFileRecordStructWithShardMapping(csv_record);
    default:
      return absl::InvalidArgumentError("Invalid record type.");
  }
}
}  // namespace internal

}  // namespace kv_server
