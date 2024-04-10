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
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/logger/request_context_impl.h"

namespace kv_server::benchmark {

// An `AsyncTask` executes a provided function, `task_fn` on an asynchronous
// thread. The asynchronous thread is automatically stopped and joined when the
// task goes out of scope or when `Stop()` is called.
class AsyncTask {
 public:
  explicit AsyncTask(std::function<void()> task_fn);
  AsyncTask(AsyncTask&& other);
  AsyncTask& operator=(AsyncTask&& other);
  AsyncTask(const AsyncTask&) = delete;
  AsyncTask& operator=(const AsyncTask&) = delete;
  ~AsyncTask();
  void Stop();

 private:
  bool stop_signal_;
  std::thread runner_thread_;
};

class BenchmarkLogContext
    : public privacy_sandbox::server_common::log::SafePathContext {
 public:
  BenchmarkLogContext() = default;
};

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
