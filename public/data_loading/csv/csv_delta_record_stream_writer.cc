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

#include "absl/status/statusor.h"
#include "public/data_loading/data_loading_generated.h"

namespace kv_server {
namespace {
absl::StatusOr<std::string_view> GetMutationType(
    const DeltaFileRecordStruct& record) {
  switch (record.mutation_type) {
    case DeltaMutationType::Update:
      return kUpdateMutationType;
    case DeltaMutationType::Delete:
      return kDeleteMutationType;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid mutation type: ",
                       EnumNameDeltaMutationType(record.mutation_type)));
  }
}
}  // namespace

namespace internal {
absl::StatusOr<riegeli::CsvRecord> MakeCsvRecord(
    const DeltaFileRecordStruct& record,
    const std::vector<std::string_view>& header) {
  // TODO: Consider using ctor with fields for performance gain if order is
  // known ahead of time.
  riegeli::CsvRecord csv_record(header);
  csv_record[kKeyColumn] = record.key;
  csv_record[kSubKeyColumn] = record.subkey;
  csv_record[kValueColumn] = record.value;
  absl::StatusOr<std::string_view> mutation_type = GetMutationType(record);
  if (!mutation_type.ok()) {
    return mutation_type.status();
  }
  csv_record[kMutationTypeColumn] = *mutation_type;
  csv_record[kLogicalCommitTimeColumn] =
      absl::StrCat(record.logical_commit_time);
  return csv_record;
}
}  // namespace internal

}  // namespace kv_server
