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
#include "absl/log/flags.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "benchmark/benchmark.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/key_value_cache.h"
#include "components/data_server/cache/noop_key_value_cache.h"
#include "components/tools/benchmarks/benchmark_util.h"
#include "components/tools/util/configure_telemetry_tools.h"

ABSL_FLAG(std::vector<std::string>, record_size,
          std::vector<std::string>({"1"}),
          "Sizes of records that we want to insert into the cache.");
ABSL_FLAG(std::vector<std::string>, set_query_size,
          std::vector<std::string>({"1"}),
          "Sizes of the set quries that we write concurrently.");
ABSL_FLAG(std::vector<std::string>, query_size, std::vector<std::string>({"1"}),
          "Number of keys that we want to read in each iteration for "
          "read benchmarks.");
ABSL_FLAG(std::vector<std::string>, keyspace_size,
          std::vector<std::string>({"1"}),
          "Size of the keyspace that we want to select keys to write from.");
ABSL_FLAG(std::vector<std::string>, concurrent_writers,
          std::vector<std::string>({"1"}),
          "Number of threads concurrently writing keys into the cache when "
          "benchmarking reads.");
ABSL_FLAG(std::vector<std::string>, concurrent_readers,
          std::vector<std::string>({"1"}),
          "Number of threads concurrently reading keys from the cache when "
          "benchmarking writes.");
ABSL_FLAG(int64_t, iterations, -1,
          "Number of iterations to run each benchmark.");
ABSL_FLAG(int64_t, min_threads, 1,
          "Minimum number of threads for benchmarking reading keys.");
ABSL_FLAG(int64_t, max_threads, 1,
          "Maximum number of threads for benchmarking reading keys.");

namespace kv_server {
namespace {

using kv_server::benchmark::AsyncTask;
using kv_server::benchmark::GenerateRandomString;
using kv_server::benchmark::ParseInt64List;

// Format variables used to generate benchmark names.
//
// => qz - query size, i.e., number of keys queried for each
// GetKeyValuePairs call.
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

constexpr std::string_view kNoOpCacheUpdateKeyValueFmt =
    "BM_NoOpCache_UpdateKeyValue/ksz:%d/rz:%d/cr:%d";
constexpr std::string_view kLockBasedCacheUpdateKeyValueFmt =
    "BM_LockBasedCache_UpdateKeyValue/ksz:%d/rz:%d/cr:%d";
constexpr std::string_view kNoOpCacheUpdateKeyValueSetFmt =
    "BM_NoOpCache_UpdateKeyValueSet/ksz:%d/sqz:%d/rz:%d/cr:%d";
constexpr std::string_view kLockBasedCacheUpdateKeyValueSetFmt =
    "BM_LockBasedCache_UpdateKeyValueSet/ksz:%d/sqz:%d/rz:%d/cr:%d";

constexpr std::string_view kReadsPerSec = "Reads/s";
constexpr std::string_view kWritesPerSec = "Writes/s";

Cache* GetNoOpCache() {
  static auto* const cache = NoOpKeyValueCache::Create().release();
  return cache;
}

Cache* GetLockBasedCache() {
  static auto* const cache = KeyValueCache::Create().release();
  return cache;
}

std::atomic<int64_t>& GetLogicalTimestamp() {
  static auto* const timestamp = new std::atomic<int64_t>(0);
  return *timestamp;
}

std::vector<std::string> GetKeys(int64_t keyset_size) {
  std::vector<std::string> keyset;
  keyset.reserve(keyset_size);
  for (int64_t i = 0; i < keyset_size; i++) {
    keyset.emplace_back(std::to_string(i));
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
  int64_t keyspace_size = 1;
  int64_t concurrent_tasks = 1;
  Cache* cache = GetNoOpCache();
};

void BM_GetKeyValuePairs(::benchmark::State& state, BenchmarkArgs args) {
  uint seed = args.concurrent_tasks;
  std::vector<AsyncTask> writer_tasks;
  benchmark::BenchmarkLogContext log_context;
  if (state.thread_index() == 0 && args.concurrent_tasks > 0) {
    auto num_writers = args.concurrent_tasks;
    writer_tasks.reserve(num_writers);
    while (num_writers-- > 0) {
      writer_tasks.emplace_back([args, &seed,
                                 value = GenerateRandomString(args.record_size),
                                 &log_context]() {
        auto key = std::to_string(rand_r(&seed) % args.query_size);
        args.cache->UpdateKeyValue(log_context, key, value,
                                   ++GetLogicalTimestamp());
      });
    }
  }
  auto keys = GetKeys(args.query_size);
  auto keys_view = ToContainerView<absl::flat_hash_set<std::string_view>>(keys);
  RequestContext request_context;
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(
        args.cache->GetKeyValuePairs(request_context, keys_view));
  }
  state.counters[std::string(kReadsPerSec)] =
      ::benchmark::Counter(state.iterations(), ::benchmark::Counter::kIsRate);
}

void BM_GetKeyValueSet(::benchmark::State& state, BenchmarkArgs args) {
  uint seed = args.concurrent_tasks;
  std::vector<AsyncTask> writer_tasks;
  benchmark::BenchmarkLogContext log_context;
  if (state.thread_index() == 0 && args.concurrent_tasks > 0) {
    auto num_writers = args.concurrent_tasks;
    writer_tasks.reserve(num_writers);
    while (num_writers-- > 0) {
      writer_tasks.emplace_back([args, &seed,
                                 set_query = GetSetQuery(args.set_query_size,
                                                         args.record_size),
                                 &log_context]() {
        auto key = std::to_string(rand_r(&seed) % args.query_size);
        auto view = ToContainerView<std::vector<std::string_view>>(set_query);
        args.cache->UpdateKeyValueSet(log_context, key, absl::MakeSpan(view),
                                      ++GetLogicalTimestamp());
      });
    }
  }
  auto keys = GetKeys(args.query_size);
  auto keys_view = ToContainerView<absl::flat_hash_set<std::string_view>>(keys);
  RequestContext request_context;
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(
        args.cache->GetKeyValueSet(request_context, keys_view));
  }
  state.counters[std::string(kReadsPerSec)] =
      ::benchmark::Counter(state.iterations(), ::benchmark::Counter::kIsRate);
}

void BM_UpdateKeyValue(::benchmark::State& state, BenchmarkArgs args) {
  uint seed = args.concurrent_tasks;
  std::vector<AsyncTask> reader_tasks;
  RequestContext request_context;
  benchmark::BenchmarkLogContext log_context;
  if (state.thread_index() == 0 && args.concurrent_tasks) {
    auto num_readers = args.concurrent_tasks;
    reader_tasks.reserve(num_readers);
    while (num_readers-- > 0) {
      reader_tasks.emplace_back([args, &seed, &request_context]() {
        auto key = std::to_string(rand_r(&seed) % args.keyspace_size);
        args.cache->GetKeyValuePairs(
            request_context, absl::flat_hash_set<std::string_view>({key}));
      });
    }
  }
  auto value = GenerateRandomString(args.record_size);
  for (auto _ : state) {
    auto key = std::to_string(rand_r(&seed) % args.keyspace_size);
    args.cache->UpdateKeyValue(log_context, key, value,
                               ++GetLogicalTimestamp());
  }
  state.counters[std::string(kWritesPerSec)] =
      ::benchmark::Counter(state.iterations(), ::benchmark::Counter::kIsRate);
}

void BM_UpdateKeyValueSet(::benchmark::State& state, BenchmarkArgs args) {
  uint seed = args.concurrent_tasks;
  std::vector<AsyncTask> reader_tasks;
  RequestContext request_context;
  benchmark::BenchmarkLogContext log_context;
  if (state.thread_index() == 0 && args.concurrent_tasks) {
    auto num_readers = args.concurrent_tasks;
    reader_tasks.reserve(num_readers);
    while (num_readers-- > 0) {
      reader_tasks.emplace_back([args, &seed, &request_context]() {
        auto key = std::to_string(rand_r(&seed) % args.keyspace_size);
        args.cache->GetKeyValueSet(request_context, {key});
      });
    }
  }
  auto set_value = GetSetQuery(args.set_query_size, args.record_size);
  auto set_view = ToContainerView<std::vector<std::string_view>>(set_value);
  for (auto _ : state) {
    auto key = std::to_string(rand_r(&seed) % args.keyspace_size);
    args.cache->UpdateKeyValueSet(log_context, key, absl::MakeSpan(set_view),
                                  ++GetLogicalTimestamp());
  }
  state.counters[std::string(kWritesPerSec)] =
      ::benchmark::Counter(state.iterations(), ::benchmark::Counter::kIsRate);
}

// Registers a function to benchmark.
void RegisterBenchmark(
    std::string name, BenchmarkArgs args,
    std::function<void(::benchmark::State&, BenchmarkArgs)> benchmark) {
  auto b =
      ::benchmark::RegisterBenchmark(name.c_str(), benchmark, std::move(args));
  auto min = std::max(absl::GetFlag(FLAGS_min_threads), 1L);
  auto max = absl::GetFlag(FLAGS_max_threads);
  b->ThreadRange(min, max < min ? min : max);
  if (absl::GetFlag(FLAGS_iterations) > 0) {
    b->Iterations(absl::GetFlag(FLAGS_iterations));
  }
}

void RegisterReadBenchmarks() {
  auto query_sizes = ParseInt64List(absl::GetFlag(FLAGS_query_size));
  auto set_query_sizes = ParseInt64List(absl::GetFlag(FLAGS_set_query_size));
  auto record_sizes = ParseInt64List(absl::GetFlag(FLAGS_record_size));
  auto concurrent_writers =
      ParseInt64List(absl::GetFlag(FLAGS_concurrent_writers));
  for (auto query_size : query_sizes.value()) {
    for (auto record_size : record_sizes.value()) {
      for (auto num_writers : concurrent_writers.value()) {
        auto args = BenchmarkArgs{
            .record_size = record_size,
            .query_size = query_size,
            .concurrent_tasks = num_writers,
            .cache = GetNoOpCache(),
        };
        ::kv_server::RegisterBenchmark(
            absl::StrFormat(kNoOpCacheGetKeyValuePairsFmt, query_size,
                            record_size, num_writers),
            args, BM_GetKeyValuePairs);
        args.cache = GetLockBasedCache();
        ::kv_server::RegisterBenchmark(
            absl::StrFormat(kLockBasedCacheGetKeyValuePairsFmt, query_size,
                            record_size, num_writers),
            args, BM_GetKeyValuePairs);
        for (auto set_query_size : set_query_sizes.value()) {
          args.set_query_size = set_query_size;
          args.cache = GetNoOpCache();
          ::kv_server::RegisterBenchmark(
              absl::StrFormat(kNoOpCacheGetKeyValueSetFmt, query_size,
                              set_query_size, record_size, num_writers),
              args, BM_GetKeyValueSet);
          args.cache = GetLockBasedCache();
          ::kv_server::RegisterBenchmark(
              absl::StrFormat(kLockBasedCacheGetKeyValueSetFmt, query_size,
                              set_query_size, record_size, num_writers),
              args, BM_GetKeyValueSet);
        }
      }
    }
  }
}

void RegisterWriteBenchmarks() {
  auto keyspace_sizes = ParseInt64List(absl::GetFlag(FLAGS_keyspace_size));
  auto record_sizes = ParseInt64List(absl::GetFlag(FLAGS_record_size));
  auto set_query_sizes = ParseInt64List(absl::GetFlag(FLAGS_set_query_size));
  auto concurrent_readers =
      ParseInt64List(absl::GetFlag(FLAGS_concurrent_readers));
  for (auto keyspace_size : keyspace_sizes.value()) {
    for (auto record_size : record_sizes.value()) {
      for (auto num_readers : concurrent_readers.value()) {
        auto args = BenchmarkArgs{
            .record_size = record_size,
            .keyspace_size = keyspace_size,
            .concurrent_tasks = num_readers,
            .cache = GetNoOpCache(),
        };
        ::kv_server::RegisterBenchmark(
            absl::StrFormat(kNoOpCacheUpdateKeyValueFmt, keyspace_size,
                            record_size, num_readers),
            args, BM_UpdateKeyValue);
        args.cache = GetLockBasedCache();
        ::kv_server::RegisterBenchmark(
            absl::StrFormat(kLockBasedCacheUpdateKeyValueFmt, keyspace_size,
                            record_size, num_readers),
            args, BM_UpdateKeyValue);
        for (auto set_query_size : set_query_sizes.value()) {
          args.set_query_size = set_query_size;
          args.cache = GetNoOpCache();
          ::kv_server::RegisterBenchmark(
              absl::StrFormat(kNoOpCacheUpdateKeyValueSetFmt, keyspace_size,
                              set_query_size, record_size, num_readers),
              args, BM_UpdateKeyValueSet);
          args.cache = GetLockBasedCache();
          ::kv_server::RegisterBenchmark(
              absl::StrFormat(kLockBasedCacheUpdateKeyValueSetFmt,
                              keyspace_size, set_query_size, record_size,
                              num_readers),
              args, BM_UpdateKeyValueSet);
        }
      }
    }
  }
}

}  // namespace
}  // namespace kv_server

// Microbenchmarks for Cache impelementations. Sample run:
//
//  bazel run -c opt \
//    //components/tools/benchmarks:cache_benchmark \
//    --config=local_instance \
//    --config=local_platform -- \
//    --benchmark_counters_tabular=true --stderrthreshold=0
int main(int argc, char** argv) {
  absl::InitializeLog();
  ::benchmark::Initialize(&argc, argv);
  absl::ParseCommandLine(argc, argv);
  kv_server::ConfigureTelemetryForTools();
  ::kv_server::RegisterReadBenchmarks();
  ::kv_server::RegisterWriteBenchmarks();
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
