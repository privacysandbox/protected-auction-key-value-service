// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/internal_server/local_lookup.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/uint_value_set.h"
#include "components/internal_server/lookup.h"
#include "components/internal_server/lookup.pb.h"
#include "components/query/driver.h"
#include "components/query/scanner.h"

namespace kv_server {
namespace {

enum class ErrorTag : int { kProcessValueSetKeys = 1 };

class LocalLookup : public Lookup {
 public:
  explicit LocalLookup(const Cache& cache) : cache_(cache) {}

  absl::StatusOr<InternalLookupResponse> GetKeyValues(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& keys) const override {
    return ProcessKeys(request_context, keys);
  }

  absl::StatusOr<InternalLookupResponse> GetKeyValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const override {
    ScopeLatencyMetricsRecorder<InternalLookupMetricsContext,
                                kInternalGetKeyValueSetLatencyInMicros>
        latency_recorder(request_context.GetInternalLookupMetricsContext());
    InternalLookupResponse response;
    if (key_set.empty()) {
      return response;
    }
    auto key_value_set_result = cache_.GetKeyValueSet(request_context, key_set);
    for (const auto& key : key_set) {
      SingleLookupResult result;
      if (const auto value_set = key_value_set_result->GetValueSet(key);
          !value_set.empty()) {
        auto* keyset_values = result.mutable_keyset_values();
        keyset_values->mutable_values()->Reserve(value_set.size());
        keyset_values->mutable_values()->Add(value_set.begin(),
                                             value_set.end());
      } else {
        auto status = result.mutable_status();
        status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
        status->set_message(absl::StrCat("Key not found: ", key));
      }
      (*response.mutable_kv_pairs())[key] = std::move(result);
    }
    return response;
  }

  absl::StatusOr<InternalLookupResponse> GetUInt32ValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const override {
    ScopeLatencyMetricsRecorder<InternalLookupMetricsContext,
                                kInternalGetUInt32ValueSetLatencyInMicros>
        latency_recorder(request_context.GetInternalLookupMetricsContext());
    InternalLookupResponse response;
    if (key_set.empty()) {
      return response;
    }
    auto key_value_set_result =
        cache_.GetUInt32ValueSet(request_context, key_set);
    for (const auto& key : key_set) {
      SingleLookupResult result;
      if (const auto value_set = key_value_set_result->GetUInt32ValueSet(key);
          value_set != nullptr && !value_set->GetValues().empty()) {
        auto uint32_values = value_set->GetValues();
        auto* result_values = result.mutable_uint32set_values();
        result_values->mutable_values()->Reserve(uint32_values.size());
        result_values->mutable_values()->Add(uint32_values.begin(),
                                             uint32_values.end());
      } else {
        auto status = result.mutable_status();
        status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
        status->set_message(absl::StrCat("Key not found: ", key));
      }
      (*response.mutable_kv_pairs())[key] = std::move(result);
    }
    return response;
  }

  absl::StatusOr<InternalLookupResponse> GetUInt64ValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const override {
    ScopeLatencyMetricsRecorder<InternalLookupMetricsContext,
                                kInternalGetUInt64ValueSetLatencyInMicros>
        latency_recorder(request_context.GetInternalLookupMetricsContext());
    InternalLookupResponse response;
    if (key_set.empty()) {
      return response;
    }
    auto key_value_set_result =
        cache_.GetUInt64ValueSet(request_context, key_set);
    for (const auto& key : key_set) {
      SingleLookupResult result;
      if (const auto value_set = key_value_set_result->GetUInt64ValueSet(key);
          value_set != nullptr && !value_set->GetValues().empty()) {
        auto uint64_values = value_set->GetValues();
        auto* result_values = result.mutable_uint64set_values();
        result_values->mutable_values()->Reserve(uint64_values.size());
        result_values->mutable_values()->Add(uint64_values.begin(),
                                             uint64_values.end());
      } else {
        auto status = result.mutable_status();
        status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
        status->set_message(absl::StrCat("Key not found: ", key));
      }
      (*response.mutable_kv_pairs())[key] = std::move(result);
    }
    return response;
  }

  absl::StatusOr<InternalRunQueryResponse> RunQuery(
      const RequestContext& request_context, std::string query) const override {
    return ProcessQuery<InternalRunQueryResponse>(
        request_context, std::move(query),
        [](const RequestContext& request_context, const Driver& driver,
           const Cache& cache) -> absl::StatusOr<InternalRunQueryResponse> {
          auto get_key_value_set_result = cache.GetKeyValueSet(
              request_context, driver.GetRootNode()->Keys());
          auto eval_result =
              driver.EvaluateQuery<absl::flat_hash_set<std::string_view>>(
                  [&get_key_value_set_result](std::string_view key) {
                    return get_key_value_set_result->GetValueSet(key);
                  });
          if (!eval_result.ok()) {
            return eval_result.status();
          }
          InternalRunQueryResponse response;
          response.mutable_elements()->Reserve(eval_result->size());
          response.mutable_elements()->Assign(eval_result->begin(),
                                              eval_result->end());
          return response;
        });
  }

  absl::StatusOr<InternalRunSetQueryUInt32Response> RunSetQueryUInt32(
      const RequestContext& request_context, std::string query) const override {
    return ProcessQuery<InternalRunSetQueryUInt32Response>(
        request_context, std::move(query),
        [](const RequestContext& request_context, const Driver& driver,
           const Cache& cache)
            -> absl::StatusOr<InternalRunSetQueryUInt32Response> {
          auto cache_result = cache.GetUInt32ValueSet(
              request_context, driver.GetRootNode()->Keys());
          auto eval_result = driver.EvaluateQuery<UInt32ValueSet::bitset_type>(
              [&cache_result](std::string_view key) {
                auto set = cache_result->GetUInt32ValueSet(key);
                return set == nullptr ? UInt32ValueSet::bitset_type()
                                      : set->GetValuesBitSet();
              });
          if (!eval_result.ok()) {
            return eval_result.status();
          }
          auto uint32_set = BitSetToUint32Set(*eval_result);
          InternalRunSetQueryUInt32Response response;
          response.mutable_elements()->Reserve(uint32_set.size());
          response.mutable_elements()->Assign(uint32_set.begin(),
                                              uint32_set.end());
          return response;
        });
  }

  absl::StatusOr<InternalRunSetQueryUInt64Response> RunSetQueryUInt64(
      const RequestContext& request_context, std::string query) const override {
    return ProcessQuery<InternalRunSetQueryUInt64Response>(
        request_context, std::move(query),
        [](const RequestContext& request_context, const Driver& driver,
           const Cache& cache)
            -> absl::StatusOr<InternalRunSetQueryUInt64Response> {
          auto cache_result = cache.GetUInt64ValueSet(
              request_context, driver.GetRootNode()->Keys());
          auto eval_result = driver.EvaluateQuery<UInt64ValueSet::bitset_type>(
              [&cache_result](std::string_view key) {
                auto set = cache_result->GetUInt64ValueSet(key);
                return set == nullptr ? UInt64ValueSet::bitset_type()
                                      : set->GetValuesBitSet();
              });
          if (!eval_result.ok()) {
            return eval_result.status();
          }
          auto uint64_set = BitSetToUint64Set(*eval_result);
          InternalRunSetQueryUInt64Response response;
          response.mutable_elements()->Reserve(uint64_set.size());
          response.mutable_elements()->Assign(uint64_set.begin(),
                                              uint64_set.end());
          return response;
        });
  }

 private:
  InternalLookupResponse ProcessKeys(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& keys) const {
    ScopeLatencyMetricsRecorder<InternalLookupMetricsContext,
                                kInternalGetKeyValuesLatencyInMicros>
        latency_recorder(request_context.GetInternalLookupMetricsContext());
    InternalLookupResponse response;
    if (keys.empty()) {
      return response;
    }
    auto kv_pairs = cache_.GetKeyValuePairs(request_context, keys);

    for (const auto& key : keys) {
      SingleLookupResult result;
      const auto key_iter = kv_pairs.find(key);
      if (key_iter == kv_pairs.end()) {
        auto status = result.mutable_status();
        status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
        status->set_message(absl::StrCat("Key not found: ", key));
      } else {
        result.set_value(std::move(key_iter->second));
      }
      (*response.mutable_kv_pairs())[key] = std::move(result);
    }
    return response;
  }

  template <typename ResponseType>
  absl::StatusOr<ResponseType> ProcessQuery(
      const RequestContext& request_context, std::string query,
      absl::AnyInvocable<absl::StatusOr<ResponseType>(
          const RequestContext&, const Driver&, const Cache&)>
          query_eval_fn) const {
    ScopeLatencyMetricsRecorder<InternalLookupMetricsContext,
                                kInternalRunQueryLatencyInMicros>
        latency_recorder(request_context.GetInternalLookupMetricsContext());
    if (query.empty()) return absl::OkStatus();
    kv_server::Driver driver;
    std::istringstream stream(std::move(query));
    kv_server::Scanner scanner(stream);
    kv_server::Parser parse(driver, scanner);
    if (int parse_result = parse(); parse_result) {
      LogInternalLookupRequestErrorMetric(
          request_context.GetInternalLookupMetricsContext(),
          kLocalRunQueryParsingFailure);
      return absl::InvalidArgumentError("Parsing failure.");
    }
    auto result = query_eval_fn(request_context, driver, cache_);
    if (!result.ok()) {
      LogInternalLookupRequestErrorMetric(
          request_context.GetInternalLookupMetricsContext(),
          kLocalRunQueryFailure);
      return result.status();
    }
    return result;
  }

  const Cache& cache_;
};

}  // namespace

std::unique_ptr<Lookup> CreateLocalLookup(const Cache& cache) {
  return std::make_unique<LocalLookup>(cache);
}

}  // namespace kv_server
