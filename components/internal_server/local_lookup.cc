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
#include "components/data_server/cache/uint32_value_set.h"
#include "components/errors/error_tag.h"
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
    return ProcessValueSetKeys(request_context, key_set,
                               SingleLookupResult::kKeysetValues);
  }

  absl::StatusOr<InternalLookupResponse> GetUInt32ValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const override {
    return ProcessValueSetKeys(request_context, key_set,
                               SingleLookupResult::kUintsetValues);
  }

  absl::StatusOr<InternalRunQueryResponse> RunQuery(
      const RequestContext& request_context, std::string query) const override {
    return ProcessQuery<InternalRunQueryResponse,
                        absl::StatusOr<absl::flat_hash_set<std::string_view>>>(
        request_context, std::move(query),
        [](const RequestContext& request_context, const Driver& driver,
           const Cache& cache) {
          auto get_key_value_set_result = cache.GetKeyValueSet(
              request_context, driver.GetRootNode()->Keys());
          return driver.EvaluateQuery<absl::flat_hash_set<std::string_view>>(
              [&get_key_value_set_result](std::string_view key) {
                return get_key_value_set_result->GetValueSet(key);
              });
        });
  }

  absl::StatusOr<InternalRunSetQueryIntResponse> RunSetQueryInt(
      const RequestContext& request_context, std::string query) const override {
    return ProcessQuery<InternalRunSetQueryIntResponse,
                        absl::StatusOr<absl::flat_hash_set<uint32_t>>>(
        request_context, std::move(query),
        [](const RequestContext& request_context, const Driver& driver,
           const Cache& cache)
            -> absl::StatusOr<absl::flat_hash_set<uint32_t>> {
          auto get_key_value_set_result = cache.GetUInt32ValueSet(
              request_context, driver.GetRootNode()->Keys());
          auto query_eval_result = driver.EvaluateQuery<roaring::Roaring>(
              [&get_key_value_set_result](std::string_view key) {
                auto set = get_key_value_set_result->GetUInt32ValueSet(key);
                return set == nullptr ? roaring::Roaring()
                                      : set->GetValuesBitSet();
              });
          if (!query_eval_result.ok()) {
            return query_eval_result.status();
          }
          return BitSetToUint32Set(*query_eval_result);
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

  absl::StatusOr<InternalLookupResponse> ProcessValueSetKeys(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set,
      SingleLookupResult::SingleLookupResultCase set_type) const {
    ScopeLatencyMetricsRecorder<InternalLookupMetricsContext,
                                kInternalGetKeyValueSetLatencyInMicros>
        latency_recorder(request_context.GetInternalLookupMetricsContext());
    InternalLookupResponse response;
    if (key_set.empty()) {
      return response;
    }
    std::unique_ptr<GetKeyValueSetResult> key_value_set_result;
    if (set_type == SingleLookupResult::kKeysetValues) {
      key_value_set_result = cache_.GetKeyValueSet(request_context, key_set);
    } else if (set_type == SingleLookupResult::kUintsetValues) {
      key_value_set_result = cache_.GetUInt32ValueSet(request_context, key_set);
    } else {
      return StatusWithErrorTag(absl::InvalidArgumentError(absl::StrCat(
                                    "Unsupported set type: ", set_type)),
                                __FILE__, ErrorTag::kProcessValueSetKeys);
    }
    for (const auto& key : key_set) {
      SingleLookupResult result;
      bool is_empty_value_set = false;
      if (set_type == SingleLookupResult::kKeysetValues) {
        if (const auto value_set = key_value_set_result->GetValueSet(key);
            !value_set.empty()) {
          auto* keyset_values = result.mutable_keyset_values();
          keyset_values->mutable_values()->Reserve(value_set.size());
          keyset_values->mutable_values()->Add(value_set.begin(),
                                               value_set.end());
        } else {
          is_empty_value_set = true;
        }
      }
      if (set_type == SingleLookupResult::kUintsetValues) {
        if (const auto value_set = key_value_set_result->GetUInt32ValueSet(key);
            value_set != nullptr && !value_set->GetValues().empty()) {
          auto uint32_values = value_set->GetValues();
          auto* result_values = result.mutable_uintset_values();
          result_values->mutable_values()->Reserve(uint32_values.size());
          result_values->mutable_values()->Add(uint32_values.begin(),
                                               uint32_values.end());
        } else {
          is_empty_value_set = true;
        }
      }
      if (is_empty_value_set) {
        auto status = result.mutable_status();
        status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
        status->set_message(absl::StrCat("Key not found: ", key));
      }
      (*response.mutable_kv_pairs())[key] = std::move(result);
    }
    return response;
  }

  template <typename ResponseType, typename QueryEvalResultType>
  absl::StatusOr<ResponseType> ProcessQuery(
      const RequestContext& request_context, std::string query,
      absl::AnyInvocable<QueryEvalResultType(const RequestContext&,
                                             const Driver&, const Cache&)>
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
    ResponseType response;
    response.mutable_elements()->Assign(result->begin(), result->end());
    return response;
  }

  const Cache& cache_;
};

}  // namespace

std::unique_ptr<Lookup> CreateLocalLookup(const Cache& cache) {
  return std::make_unique<LocalLookup>(cache);
}

}  // namespace kv_server
