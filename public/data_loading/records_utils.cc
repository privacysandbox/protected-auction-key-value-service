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

namespace kv_server {
namespace {
// An arbitrary small number in case the flat buffer needs some space for
// overheads.
constexpr int kOverheadSize = 10;
}  // namespace

std::string_view ToStringView(const flatbuffers::FlatBufferBuilder& fb_buffer) {
  return std::string_view(reinterpret_cast<char*>(fb_buffer.GetBufferPointer()),
                          fb_buffer.GetSize());
}

flatbuffers::FlatBufferBuilder DeltaFileRecordStruct::ToFlatBuffer() const {
  flatbuffers::FlatBufferBuilder builder(
      key.size() + subkey.size() + value.size() + sizeof(logical_commit_time) +
      sizeof(mutation_type) + kOverheadSize);
  const auto record =
      CreateDeltaFileRecordDirect(builder, mutation_type, logical_commit_time,
                                  key.data(), subkey.data(), value.data());
  builder.Finish(record);
  return builder;
}

bool operator==(const DeltaFileRecordStruct& lhs_record,
                const DeltaFileRecordStruct& rhs_record) {
  return lhs_record.logical_commit_time == rhs_record.logical_commit_time &&
         lhs_record.mutation_type == rhs_record.mutation_type &&
         lhs_record.key == rhs_record.key &&
         lhs_record.subkey == rhs_record.subkey &&
         lhs_record.value == rhs_record.value;
}

bool operator!=(const DeltaFileRecordStruct& lhs_record,
                const DeltaFileRecordStruct& rhs_record) {
  return !operator==(lhs_record, rhs_record);
}

bool operator==(const DeltaFileRecordStructKey& lhs_key,
                const DeltaFileRecordStructKey& rhs_key) {
  return lhs_key.key == rhs_key.key && lhs_key.subkey == rhs_key.subkey;
}
}  // namespace kv_server
