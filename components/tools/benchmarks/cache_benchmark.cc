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
#include <array>
#include <chrono>
#include <future>
#include <memory>
#include <sstream>
#include <thread>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "benchmark/benchmark.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/key_value_cache.h"
#include "components/data_server/cache/noop_key_value_cache.h"
#include "components/tools/benchmarks/benchmark_util.h"
#include "glog/logging.h"

ABSL_FLAG(std::vector<std::string>, record_size,
          std::vector<std::string>({"1"}),
          "Sizes of records that we want to insert into the cache.");
ABSL_FLAG(std::vector<std::string>, set_query_size,
          std::vector<std::string>({"1"}),
          "Sizes of the set quries that we write concurrently.");
ABSL_FLAG(std::vector<std::string>, query_size, std::vector<std::string>({"1"}),
          "Number of keys that we want to read in each iteration for "
          "read benchmarks.");
ABSL_FLAG(std::vector<std::string>, concurrent_writers,
          std::vector<std::string>({"0"}),
          "Number of threads concurrently writing keys into the cache.");
ABSL_FLAG(int64_t, iterations, -1,
          "Number of iterations to run each benchmark.");
ABSL_FLAG(int64_t, min_concurrent_readers, 1,
          "Minimum number of threads for benchmarking reading keys.");
ABSL_FLAG(int64_t, max_concurrent_readers, 1,
          "Maximum number of threads for benchmarking reading keys.");

using kv_server::Cache;
using kv_server::KeyValueCache;
using kv_server::NoOpKeyValueCache;
using kv_server::benchmark::AsyncTask;
using kv_server::benchmark::GenerateRandomString;
using kv_server::benchmark::ParseInt64List;

// Format variables used to generate benchmark names.
//
// => qz - query size, i.e., number of keys queried for each GetKeyValuePairs
// call.
// => rz - record size, i.e., approximate byte size of each key/value pair
// written into the cache. Actual record size is greater than this number.
constexpr std::string_view kNoOpCacheGetKeyValuePairsFmt =
    "BM_NoOpCache_GetKeyValuePairs/qz:%d/rz:%d/cw:%d";
constexpr std::string_view kLockBasedCacheGetKeyValuePairsFmt =
    "BM_LockBasedCache_GetKeyValuePairs/qz:%d/rz:%d/cw:%d";
constexpr std::string_view kNoOpCacheGetKeyValueSetFmt =
    "BM_NoOpCache_GetKeyValueSet/qz:%d/sqz:%d/rz:%d/cw:%d";
constexpr std::string_view kLockBasedCacheGetKeyValueSetFmt =
    "BM_LockBasedCache_GetKeyValueSet/qz:%d/sqz:%d/rz:%d/cw:%d";

constexpr std::string_view kKeyFormat = "key%d";
constexpr std::string_view kReadsPerSec = "Reads/s";

Cache& GetNoOpCache() {
  static auto* const cache = NoOpKeyValueCache::Create().release();
  return *cache;
}

Cache& GetLockBasedCache() {
  static auto* const cache = KeyValueCache::Create().release();
  return *cache;
}

std::atomic<int64_t>& GetLogicalTimestamp() {
  static auto* const timestamp = new std::atomic<int64_t>(0);
  return *timestamp;
}

std::vector<std::string> GetKeys(int64_t keyset_size) {
  std::vector<std::string> keyset;
  keyset.reserve(keyset_size);
  for (int64_t i = 0; i < keyset_size; i++) {
    keyset.emplace_back(absl::StrFormat(kKeyFormat, i));
  }
  return keyset;
}

std::vector<std::string> GetSetQuery(int64_t num_elems, int64_t elem_size) {
  std::vector<std::string> set_query;
  set_query.reserve(num_elems);
  for (int64_t i = 0; i < num_elems; i++) {
    set_query.push_back(GenerateRandomString(elem_size));
  }
  return set_query;
}

template <typename ContainerT>
ContainerT ToContainerView(const std::vector<std::string>& list) {
  ContainerT container;
  container.reserve(list.size());
  for (const auto& key : list) {
    if constexpr (std::is_same_v<ContainerT, std::vector<std::string_view>>) {
      container.emplace_back(key);
    }
    if constexpr (std::is_same_v<ContainerT,
                                 absl::flat_hash_set<std::string_view>>) {
      container.insert(key);
    }
  }
  return container;
}

struct BenchmarkArgs {
  int64_t record_size = 1;
  int64_t query_size = 1;
  int64_t set_query_size = 1;
  int64_t concurrent_writers = 0;
  Cache& cache = GetNoOpCache();
};

void BM_GetKeyValuePairs(benchmark::State& state, BenchmarkArgs args) {
  std::vector<AsyncTask> writer_tasks;
  if (state.thread_index() == 0 && args.concurrent_writers > 0) {
    auto num_writers = args.concurrent_writers;
    writer_tasks.reserve(num_writers);
    while (num_writers-- > 0) {
      writer_tasks.emplace_back(
          [args, value = GenerateRandomString(args.record_size)]() {
            auto key =
                absl::StrFormat(kKeyFormat, std::rand() % args.query_size);
            args.cache.UpdateKeyValue(key, value, ++GetLogicalTimestamp());
          });
    }
  }
  auto keys = GetKeys(args.query_size);
  auto keys_view = ToContainerView<std::vector<std::string_view>>(keys);
  for (auto _ : state) {
    benchmark::DoNotOptimize(args.cache.GetKeyValuePairs(keys_view));
  }
  state.counters[std::string(kReadsPerSec)] =
      benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}

void BM_GetKeyValueSet(benchmark::State& state, BenchmarkArgs args) {
  std::vector<AsyncTask> writer_tasks;
  if (state.thread_index() == 0 && args.concurrent_writers > 0) {
    auto num_writers = args.concurrent_writers;
    writer_tasks.reserve(num_writers);
    while (num_writers-- > 0) {
      writer_tasks.emplace_back([args,
                                 set_query = GetSetQuery(args.set_query_size,
                                                         args.record_size)]() {
        auto key = absl::StrFormat(kKeyFormat, std::rand() % args.query_size);
        auto view = ToContainerView<std::vector<std::string_view>>(set_query);
        args.cache.UpdateKeyValueSet(key, absl::MakeSpan(view),
                                     ++GetLogicalTimestamp());
      });
    }
  }
  auto keys = GetKeys(args.query_size);
  auto keys_view = ToContainerView<absl::flat_hash_set<std::string_view>>(keys);
  for (auto _ : state) {
    benchmark::DoNotOptimize(args.cache.GetKeyValueSet(keys_view));
  }
  state.counters[std::string(kReadsPerSec)] =
      benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}

// Registers a function to benchmark.
void RegisterBenchmark(
    std::string name, BenchmarkArgs args,
    std::function<void(benchmark::State&, BenchmarkArgs)> benchmark) {
  auto b =
      benchmark::RegisterBenchmark(name.c_str(), benchmark, std::move(args));
  auto min = absl::GetFlag(FLAGS_min_concurrent_readers);
  auto max = absl::GetFlag(FLAGS_max_concurrent_readers);
  b->ThreadRange(min, max < min ? min : max);
  if (absl::GetFlag(FLAGS_iterations) > 0) {
    b->Iterations(absl::GetFlag(FLAGS_iterations));
  }
}

void RegisterBenchmarks() {
  auto query_sizes = ParseInt64List(absl::GetFlag(FLAGS_query_size));
  auto set_query_sizes = ParseInt64List(absl::GetFlag(FLAGS_set_query_size));
  auto record_sizes = ParseInt64List(absl::GetFlag(FLAGS_record_size));
  auto concurrent_writers =
      ParseInt64List(absl::GetFlag(FLAGS_concurrent_writers));
  for (auto query_size : query_sizes.value()) {
    for (auto record_size : record_sizes.value()) {
      for (auto num_writers : concurrent_writers.value()) {
        RegisterBenchmark(absl::StrFormat(kNoOpCacheGetKeyValuePairsFmt,
                                          query_size, record_size, num_writers),
                          BenchmarkArgs{
                              .record_size = record_size,
                              .query_size = query_size,
                              .concurrent_writers = num_writers,
                              .cache = GetNoOpCache(),
                          },
                          BM_GetKeyValuePairs);
        RegisterBenchmark(absl::StrFormat(kLockBasedCacheGetKeyValuePairsFmt,
                                          query_size, record_size, num_writers),
                          BenchmarkArgs{
                              .record_size = record_size,
                              .query_size = query_size,
                              .concurrent_writers = num_writers,
                              .cache = GetLockBasedCache(),
                          },
                          BM_GetKeyValuePairs);
        for (auto set_query_size : set_query_sizes.value()) {
          RegisterBenchmark(
              absl::StrFormat(kNoOpCacheGetKeyValueSetFmt, query_size,
                              set_query_size, record_size, num_writers),
              BenchmarkArgs{
                  .record_size = record_size,
                  .query_size = query_size,
                  .set_query_size = set_query_size,
                  .concurrent_writers = num_writers,
                  .cache = GetNoOpCache(),
              },
              BM_GetKeyValueSet);
          RegisterBenchmark(
              absl::StrFormat(kLockBasedCacheGetKeyValueSetFmt, query_size,
                              set_query_size, record_size, num_writers),
              BenchmarkArgs{
                  .record_size = record_size,
                  .query_size = query_size,
                  .set_query_size = set_query_size,
                  .concurrent_writers = num_writers,
                  .cache = GetLockBasedCache(),
              },
              BM_GetKeyValueSet);
        }
      }
    }
  }
}

// Microbenchmarks for Cache impelementations. Sample run:
//
//  GLOG_logtostderr=1 bazel run -c opt \
//    //components/tools/benchmarks:cache_benchmark \
//    --//:instance=local \
//    --//:platform=local -- \
//    --benchmark_counters_tabular=true
int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  ::benchmark::Initialize(&argc, argv);
  absl::ParseCommandLine(argc, argv);
  RegisterBenchmarks();
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
