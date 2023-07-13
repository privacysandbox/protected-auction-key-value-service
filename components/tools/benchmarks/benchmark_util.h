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

#ifndef COMPONENTS_TOOLS_BENCHMARKS_BENCHMARK_UTIL_H_
#define COMPONENTS_TOOLS_BENCHMARKS_BENCHMARK_UTIL_H_

#include <iostream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace kv_server::benchmark {

// Generates a random string with `char_count` characters.
std::string GenerateRandomString(const int64_t char_count);

// Write num_records, each with a size of record_size, to output_stream.
absl::Status WriteRecords(int64_t num_records, int64_t record_size,
                          std::iostream& output_stream);

// Parses a numeric string list into a vector of int64 elements.
absl::StatusOr<std::vector<int64_t>> ParseInt64List(
    const std::vector<std::string>& num_list);

}  // namespace kv_server::benchmark

#endif  // COMPONENTS_TOOLS_BENCHMARKS_BENCHMARK_UTIL_H_
