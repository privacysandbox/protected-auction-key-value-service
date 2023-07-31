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
#include "components/tools/benchmarks/benchmark_util.h"
#include "glog/logging.h"

ABSL_FLAG(std::vector<std::string>, args_record_size,
          std::vector<std::string>({"100"}),
          "Sizes of records that we want to insert into the cache.");
ABSL_FLAG(std::vector<std::string>, args_dataset_size,
          std::vector<std::string>({"1000"}),
          "Number of unique key/value pairs in the cache.");
ABSL_FLAG(std::vector<std::string>, args_reads_keyset_size,
          std::vector<std::string>({"100"}),
          "Sizes of the keyset that we want to read in each iteration for "
          "read benchmarks.");
ABSL_FLAG(int64_t, args_benchmark_iterations, -1,
          "Number of iterations to run each benchmark.");
ABSL_FLAG(int64_t, args_benchmark_max_concurrent_readers, -1,
          "Maximum number of threads for benchmarking.");

using kv_server::Cache;
using kv_server::GetKeyValueSetResult;
using kv_server::KeyValueCache;
using kv_server::benchmark::GenerateRandomString;
using kv_server::benchmark::ParseInt64List;

// Format strings used to generate benchmark names. The "dsz", "ksz" and "rsz"
// components in the benchmark name represents the following:
// => dsz - dataset size, i.e., number of key/value pairs in cache when the
// benchmark was run.
// => ksz - keyset size, i.e., number of keys queried for each GetKeyValuePairs
// call.
// => rsz - record size, i.e., approximate byte size of each key/value pair in
// cache.
constexpr std::string_view kNoOpCacheGetKeyValuesFmt =
    "BM_NoOpCache_GetKeyValuePairs/dsz:%d/ksz:%d/rsz:%d";
constexpr std::string_view kKeyValueCacheGetKeyValuesFmt =
    "BM_KeyValueCache_GetKeyValuePairs/dsz:%d/ksz:%d/rsz:%d";

class NoOpCache : public Cache {
 public:
  absl::flat_hash_map<std::string, std::string> GetKeyValuePairs(
      const std::vector<std::string_view>& key_list) const override {
    return {};
  };
  std::unique_ptr<kv_server::GetKeyValueSetResult> GetKeyValueSet(
      const absl::flat_hash_set<std::string_view>& key_set) const override {
    return std::make_unique<NoOpGetKeyValueSetResult>();
  }
  void UpdateKeyValue(std::string_view key, std::string_view value,
                      int64_t logical_commit_time) override {}
  void UpdateKeyValueSet(std::string_view key,
                         absl::Span<std::string_view> value_set,
                         int64_t logical_commit_time) override {}
  void DeleteKey(std::string_view key, int64_t logical_commit_time) override {}
  void DeleteValuesInSet(std::string_view key,
                         absl::Span<std::string_view> value_set,
                         int64_t logical_commit_time) override {}
  void RemoveDeletedKeys(int64_t logical_commit_time) override {}
  static std::unique_ptr<Cache> Create() {
    return std::make_unique<NoOpCache>();
  }

 private:
  class NoOpGetKeyValueSetResult : public GetKeyValueSetResult {
    absl::flat_hash_set<std::string_view> GetValueSet(
        std::string_view key) const override {
      return {};
    }
    void AddKeyValueSet(
        absl::Mutex& key_mutex, std::string_view key,
        absl::flat_hash_set<std::string_view> value_set) override {}
  };
};

static Cache* shared_cache = nullptr;
void InitSharedNoOpCache(const benchmark::State&) {
  shared_cache = NoOpCache::Create().release();
}

void InitSharedKeyValueCache(const benchmark::State&) {
  shared_cache = KeyValueCache::Create().release();
}

void DeleteSharedCache(const benchmark::State&) { delete shared_cache; }

struct BenchmarkArgs {
  int64_t record_size;
  int64_t reads_keyset_size;
  int64_t dataset_size;
};

class CacheReadsBenchmark {
 public:
  CacheReadsBenchmark(BenchmarkArgs args, Cache& cache)
      : args_(args), cache_(cache) {}

  std::vector<std::string> PreLoadDataIntoCache() {
    for (int i = 0; i < args_.dataset_size; i++) {
      auto&& key = absl::StrCat("key", i);
      auto&& value = GenerateRandomString(args_.record_size);
      cache_.UpdateKeyValue(key, value, 10);
    }
    std::vector<std::string> keyset;
    for (int i = 0; i < args_.reads_keyset_size; i++) {
      keyset.push_back(absl::StrCat("key", std::rand() % args_.dataset_size));
    }
    return keyset;
  }

  void RunGetKeyValuePair(const std::vector<std::string_view>& keyset,
                          benchmark::State& state) {
    for (auto _ : state) {
      auto result = cache_.GetKeyValuePairs(keyset);
      benchmark::DoNotOptimize(result);
    }
  }

 private:
  BenchmarkArgs args_;
  Cache& cache_;
};

std::vector<std::string_view> ToStringViewList(
    const std::vector<std::string>& keyset) {
  std::vector<std::string_view> keyset_view;
  for (const auto& key : keyset) {
    keyset_view.emplace_back(key);
  }
  return keyset_view;
}

void BM_Cache_GetKeyValuePairs(benchmark::State& state, BenchmarkArgs args) {
  std::vector<std::string> keyset;
  static CacheReadsBenchmark* cache_benchmark = nullptr;
  if (state.thread_index() == 0) {
    cache_benchmark = new CacheReadsBenchmark(args, *shared_cache);
    keyset = cache_benchmark->PreLoadDataIntoCache();
  }
  cache_benchmark->RunGetKeyValuePair(ToStringViewList(keyset), state);
}

// Registers a function to benchmark.
void RegisterBenchmark(
    std::string name, BenchmarkArgs args,
    std::function<void(benchmark::State&, BenchmarkArgs)> benchmark,
    void (*benchmark_setup_fn)(const benchmark::State&) = nullptr,
    void (*benchmark_teardown_fn)(const benchmark::State&) = nullptr) {
  auto b = benchmark::RegisterBenchmark(name.c_str(), benchmark, args);
  if (benchmark_setup_fn) {
    b->Setup(benchmark_setup_fn);
  }
  if (benchmark_teardown_fn) {
    b->Teardown(benchmark_teardown_fn);
  }
  if (absl::GetFlag(FLAGS_args_benchmark_iterations) > 0) {
    b->Iterations(absl::GetFlag(FLAGS_args_benchmark_iterations));
  }
  if (absl ::GetFlag(FLAGS_args_benchmark_max_concurrent_readers) > 0) {
    b->ThreadRange(1,
                   absl::GetFlag(FLAGS_args_benchmark_max_concurrent_readers));
  }
}

void RegisterCacheReadsBenchmarks() {
  auto record_sizes = ParseInt64List(absl::GetFlag(FLAGS_args_record_size));
  auto keyset_sizes =
      ParseInt64List(absl::GetFlag(FLAGS_args_reads_keyset_size));
  auto dataset_sizes = ParseInt64List(absl::GetFlag(FLAGS_args_dataset_size));
  for (auto dataset_size : dataset_sizes.value()) {
    for (auto keyset_size : keyset_sizes.value()) {
      for (auto record_size : record_sizes.value()) {
        auto args = BenchmarkArgs{
            .record_size = record_size,
            .reads_keyset_size = keyset_size,
            .dataset_size = dataset_size,
        };
        RegisterBenchmark(
            absl::StrFormat(kNoOpCacheGetKeyValuesFmt, dataset_size,
                            keyset_size, record_size),
            args, BM_Cache_GetKeyValuePairs, InitSharedNoOpCache,
            DeleteSharedCache);
        RegisterBenchmark(
            absl::StrFormat(kKeyValueCacheGetKeyValuesFmt, dataset_size,
                            keyset_size, record_size),
            args, BM_Cache_GetKeyValuePairs, InitSharedKeyValueCache,
            DeleteSharedCache);
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
  RegisterCacheReadsBenchmarks();
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
