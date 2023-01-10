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

namespace kv_server {
namespace {
absl::StatusOr<int64_t> GetLogicalCommitTime(
    absl::string_view logical_commit_time) {
  if (int64_t result; absl::SimpleAtoi(logical_commit_time, &result)) {
    return result;
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "Cannot convert timestamp:", logical_commit_time, " to a number."));
}

absl::StatusOr<DeltaMutationType> GetDeltaMutationType(
    absl::string_view mutation_type) {
  std::string mt_lower = absl::AsciiStrToLower(mutation_type);
  if (mt_lower == kUpdateMutationType) {
    return DeltaMutationType::Update;
  }
  if (mt_lower == kDeleteMutationType) {
    return DeltaMutationType::Delete;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Unknown mutation type:", mutation_type));
}
}  // namespace

namespace internal {
absl::StatusOr<DeltaFileRecordStruct> MakeDeltaFileRecordStruct(
    const riegeli::CsvRecord& csv_record) {
  DeltaFileRecordStruct record;
  record.key = csv_record[kKeyColumn];
  record.subkey = csv_record[kSubKeyColumn];
  record.value = csv_record[kValueColumn];
  absl::StatusOr<int64_t> commit_time =
      GetLogicalCommitTime(csv_record[kLogicalCommitTimeColumn]);
  if (!commit_time.ok()) {
    return commit_time.status();
  }
  record.logical_commit_time = *commit_time;
  absl::StatusOr<DeltaMutationType> mutation_type =
      GetDeltaMutationType(csv_record[kMutationTypeColumn]);
  if (!mutation_type.ok()) {
    return mutation_type.status();
  }
  record.mutation_type = *mutation_type;
  return record;
}
}  // namespace internal

}  // namespace kv_server
