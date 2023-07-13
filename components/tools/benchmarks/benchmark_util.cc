/*
 * Copyright 2023 Google LLC
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

#include "components/tools/benchmarks/benchmark_util.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "public/data_loading/records_utils.h"
#include "public/data_loading/writers/delta_record_stream_writer.h"

namespace kv_server::benchmark {

std::string GenerateRandomString(const int64_t char_count) {
  return std::string(char_count, 'A' + (std::rand() % 15));
}

absl::Status WriteRecords(int64_t num_records, const int64_t record_size,
                          std::iostream& output_stream) {
  auto record_writer = DeltaRecordStreamWriter<>::Create(
      output_stream, DeltaRecordWriter::Options{});
  if (!record_writer.ok()) {
    return record_writer.status();
  }
  while (num_records > 0) {
    const std::string key = absl::StrCat("foo", num_records);
    const std::string value = GenerateRandomString(record_size);
    auto kv_mutation_record = KeyValueMutationRecordStruct{
        .mutation_type = KeyValueMutationType::Update,
        .logical_commit_time = absl::ToUnixSeconds(absl::Now()),
        .key = key,
        .value = value,
    };
    auto status = (*record_writer)
                      ->WriteRecord(DataRecordStruct{
                          .record = std::move(kv_mutation_record)});
    if (!status.ok()) {
      return status;
    }
    --num_records;
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<int64_t>> ParseInt64List(
    const std::vector<std::string>& num_list) {
  std::vector<int64_t> result;
  for (std::string_view num_string : num_list) {
    int64_t num;
    if (!absl::SimpleAtoi(num_string, &num)) {
      return absl::InvalidArgumentError("Failed to parse list into numbers.");
    }
    result.push_back(num);
  }
  return result;
}

}  // namespace kv_server::benchmark
