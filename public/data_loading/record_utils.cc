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

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"

namespace kv_server {
namespace {

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

}  // namespace

absl::Status DeserializeRecord(
    std::string_view record_bytes,
    const std::function<absl::Status(const KeyValueMutationRecord&)>&
        record_callback) {
  auto fbs_record =
      DeserializeAndVerifyRecord<KeyValueMutationRecord>(record_bytes);
  if (!fbs_record.ok()) {
    return fbs_record.status();
  }
  if (fbs_record.value()->value_type() == Value::NONE) {
    return absl::InvalidArgumentError("Record value is not set.");
  }
  return record_callback(**fbs_record);
}

absl::Status DeserializeRecord(
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

template <>
absl::StatusOr<std::string_view> MaybeGetRecordValue(
    const KeyValueMutationRecord& record) {
  const kv_server::StringValue* maybe_value = record.value_as_StringValue();
  if (!maybe_value) {
    return absl::InvalidArgumentError(absl::StrCat(
        "KeyValueMutationRecord does not contain expected value type. "
        "Expected: String",
        ". Actual: ", EnumNameValue(record.value_type())));
  }

  return maybe_value->value()->string_view();
}

template <>
absl::StatusOr<std::vector<std::string_view>> MaybeGetRecordValue(
    const KeyValueMutationRecord& record) {
  std::vector<std::string_view> values;
  const kv_server::StringSet* maybe_value = record.value_as_StringSet();
  if (!maybe_value) {
    return absl::InvalidArgumentError(absl::StrCat(
        "KeyValueMutationRecord does not contain expected value type. "
        "Expected: StringSet",
        ". Actual: ", EnumNameValue(record.value_type())));
  }

  for (const auto& val : *maybe_value->value()) {
    values.push_back(val->string_view());
  }
  return values;
}

template <>
absl::StatusOr<std::vector<uint32_t>> MaybeGetRecordValue(
    const KeyValueMutationRecord& record) {
  const kv_server::UInt32Set* maybe_value = record.value_as_UInt32Set();
  if (!maybe_value) {
    return absl::InvalidArgumentError(absl::StrCat(
        "KeyValueMutationRecord does not contain expected value type. "
        "Expected: UInt32Set",
        ". Actual: ", EnumNameValue(record.value_type())));
  }
  return std::vector<uint32_t>(maybe_value->value()->begin(),
                               maybe_value->value()->end());
}

}  // namespace kv_server
