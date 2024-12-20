/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PUBLIC_TEST_UTIL_DATA_RECORD_H_
#define PUBLIC_TEST_UTIL_DATA_RECORD_H_

#include <string>
#include <utility>
#include <vector>

namespace kv_server {

StringValueT GetSimpleStringValue(std::string value = "value") {
  return StringValueT{.value = value};
}

StringSetT GetStringSetValue(const std::vector<std::string_view>& values) {
  return StringSetT{.value = {values.begin(), values.end()}};
}

template <typename ElementType, typename UIntSetType>
UIntSetType GetUIntSetValue(const std::vector<ElementType>& values) {
  return UIntSetType{.value = {values.begin(), values.end()}};
}

template <typename ValueT>
KeyValueMutationRecordT GetKVMutationRecord(ValueT&& value) {
  KeyValueMutationRecordT record = KeyValueMutationRecordT{
      .mutation_type = KeyValueMutationType::Update,
      .logical_commit_time = 1234567890,
      .key = "key",
  };
  record.value.Set(std::move(value));
  return record;
}

UserDefinedFunctionsConfigT GetUserDefinedFunctionsConfig() {
  UserDefinedFunctionsConfigT udf_config_record = {
      .language = UserDefinedFunctionsLanguage::Javascript,
      .code_snippet = "function hello(){}",
      .handler_name = "hello",
      .logical_commit_time = 1234567890,
      .version = 1,
  };
  return udf_config_record;
}

template <typename T>
DataRecordT GetNativeDataRecord(T&& record) {
  DataRecordT data_record;
  data_record.record.Set(std::move(record));
  return data_record;
}

}  // namespace kv_server

#endif  // PUBLIC_TEST_UTIL_DATA_RECORD_H_
