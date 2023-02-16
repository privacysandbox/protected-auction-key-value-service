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
#include <utility>

#include "public/data_loading/data_loading_generated.h"

namespace kv_server {

std::string_view ToStringView(const flatbuffers::FlatBufferBuilder& fb_buffer);

struct DeltaFileRecordStruct {
  kv_server::DeltaMutationType mutation_type;
  int64_t logical_commit_time;
  std::string_view key;
  std::string_view value;

  flatbuffers::FlatBufferBuilder ToFlatBuffer() const;
};

bool operator==(const DeltaFileRecordStruct& lhs_record,
                const DeltaFileRecordStruct& rhs_record);

bool operator!=(const DeltaFileRecordStruct& lhs_record,
                const DeltaFileRecordStruct& rhs_record);

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_RECORDS_UTILS_H_
