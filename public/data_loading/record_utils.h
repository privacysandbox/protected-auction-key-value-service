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

#ifndef PUBLIC_DATA_LOADING_RECORD_UTILS_H_
#define PUBLIC_DATA_LOADING_RECORD_UTILS_H_

#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "flatbuffers/flatbuffer_builder.h"
#include "public/data_loading/data_loading_generated.h"

namespace kv_server {

enum class CsvEncoding : int { kPlaintext, kBase64 };

// Casts the flat buffer `record_buffer` into a string representation.
inline std::string_view ToStringView(
    const ::flatbuffers::FlatBufferBuilder& record_buffer) {
  return std::string_view(
      reinterpret_cast<const char*>(record_buffer.GetBufferPointer()),
      record_buffer.GetSize());
}

// Builds a Flatbuffers object from a C++ native object (defined by the
// Flatbuffers generated C++ code. See unit tests for examples.)
template <typename T>
::flatbuffers::FlatBufferBuilder FlatBufferObjectFromStruct(
    const T& native_object) {
  static_assert(
      std::is_base_of<flatbuffers::Table, typename T::TableType>::value,
      "T must be a flatbuffer::Table");

  ::flatbuffers::FlatBufferBuilder builder;
  ::flatbuffers::Offset<typename T::TableType> offset =
      T::TableType::Pack(builder, &native_object);
  builder.Finish(offset);
  return builder;
}

// Serializes a C++ native object of a Flatbuffers object into Flatbuffers
// representation and a string_view. The FlatBufferBuilder owns the memory.
template <typename T>
std::pair<::flatbuffers::FlatBufferBuilder, std::string_view> Serialize(
    const T& struct_object) {
  auto fbs_builder = FlatBufferObjectFromStruct(struct_object);
  auto string_view = ToStringView(fbs_builder);
  return std::make_pair(std::move(fbs_builder), string_view);
}

// Deserializes "data_loading.fbs:KeyValueMutationRecord" raw flatbuffer
// record bytes and calls `record_callback` with the resulting
// `KeyValueMutationRecord` object. Returns `absl::InvalidArgumentError` if
// deserialization fails, otherwise returns the result of calling
// `record_callback`.
absl::Status DeserializeRecord(
    std::string_view record_bytes,
    const std::function<absl::Status(const KeyValueMutationRecord&)>&
        record_callback);

// Deserializes "data_loading.fbs:DataRecord" raw flatbuffer record
// bytes and calls `record_callback` with the resulting `DataRecord`
// object.
// Returns `absl::InvalidArgumentError` if deserialization fails, otherwise
// returns the result of calling `record_callback`.
absl::Status DeserializeRecord(
    std::string_view record_bytes,
    const std::function<absl::Status(const DataRecord&)>& record_callback);

// Utility function to get the union value set on the `record`. Must
// be called after checking the type of the union value using
// `record.value_type()` function.
//
// Only string and string_set have implementations. See below.
template <typename ValueT>
absl::StatusOr<ValueT> MaybeGetRecordValue(
    const KeyValueMutationRecord& record);

// Returns the string value stored in `record.value`. Returns error if the
// record.value is not a string.
template <>
absl::StatusOr<std::string_view> MaybeGetRecordValue(
    const KeyValueMutationRecord& record);

// Returns the vector of strings stored in `record.value`. Returns error if the
// record.value is not a string set.
template <>
absl::StatusOr<std::vector<std::string_view>> MaybeGetRecordValue(
    const KeyValueMutationRecord& record);

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_RECORD_UTILS_H_
