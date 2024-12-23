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

#include "absl/hash/hash.h"
#include "absl/strings/str_cat.h"
#include "benchmark/benchmark.h"
#include "public/data_loading/aggregation/record_aggregator.h"
#include "public/data_loading/record_utils.h"
#include "public/data_loading/records_utils.h"

using kv_server::KeyValueMutationType;
using kv_server::RecordAggregator;

static std::string GenerateRecordValue(int64_t char_count) {
  return std::string(char_count, 'A' + (std::rand() % 15));
}

static void BM_InMemoryRecordAggregator_InsertRecord(benchmark::State& state) {
  auto record_aggregator = RecordAggregator::CreateInMemoryAggregator();
  kv_server::StringValueT string_value;
  string_value.value = GenerateRecordValue(state.range(0));
  kv_server::KeyValueMutationRecordT record;
  record.mutation_type = KeyValueMutationType::Update;
  record.logical_commit_time = 1234567890;
  record.value.Set(string_value);
  for (auto _ : state) {
    state.PauseTiming();
    std::string record_key = absl::StrCat("key", std::rand() % 10'000);
    record.key = record_key;
    size_t record_hash = absl::HashOf(record.key);
    state.ResumeTiming();
    auto ignored =
        (*record_aggregator)->InsertOrUpdateRecord(record_hash, record);
  }
  state.SetBytesProcessed(state.range(0) * state.iterations());
}

BENCHMARK(BM_InMemoryRecordAggregator_InsertRecord)->Range(64, 8192);

BENCHMARK_MAIN();
