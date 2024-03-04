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
#include "components/internal_server/sharded_lookup.h"

#include <algorithm>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "components/internal_server/lookup.h"
#include "components/internal_server/lookup.pb.h"
#include "components/internal_server/remote_lookup_client.h"
#include "components/query/driver.h"
#include "components/query/scanner.h"
#include "components/sharding/shard_manager.h"
#include "components/util/request_context.h"
#include "pir/hashing/sha256_hash_family.h"

namespace kv_server {
namespace {

using google::protobuf::RepeatedPtrField;

void UpdateResponse(
    const std::vector<std::string_view>& key_list,
    ::google::protobuf::Map<std::string, ::kv_server::SingleLookupResult>&
        kv_pairs,
    InternalLookupResponse& response) {
  for (const auto& key : key_list) {
    const auto key_iter = kv_pairs.find(key);
    if (key_iter == kv_pairs.end()) {
      SingleLookupResult result;
      auto status = result.mutable_status();
      status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
      (*response.mutable_kv_pairs())[key] = std::move(result);
    } else {
      (*response.mutable_kv_pairs())[key] = std::move(key_iter->second);
    }
  }
}

void SetRequestFailed(const std::vector<std::string_view>& key_list,
                      InternalLookupResponse& response) {
  SingleLookupResult result;
  auto status = result.mutable_status();
  status->set_code(static_cast<int>(absl::StatusCode::kInternal));
  status->set_message("Data lookup failed");
  for (const auto& key : key_list) {
    (*response.mutable_kv_pairs())[key] = result;
  }
  LOG(ERROR) << "Sharded lookup failed:" << response.DebugString();
}

class ShardedLookup : public Lookup {
 public:
  explicit ShardedLookup(const Lookup& local_lookup, const int32_t num_shards,
                         const int32_t current_shard_num,
                         const ShardManager& shard_manager,
                         KeySharder key_sharder)
      : local_lookup_(local_lookup),
        num_shards_(num_shards),
        current_shard_num_(current_shard_num),
        shard_manager_(shard_manager),
        key_sharder_(std::move(key_sharder)) {
    CHECK_GT(num_shards, 1) << "num_shards for ShardedLookup must be > 1";
  }

  // Iterates over all keys specified in the `request` and assigns them to shard
  // buckets. Then for each bucket it queries the underlying data shard. At the
  // moment, for the shard number matching the current server shard number, the
  // logic will lookup data in its own cache. Eventually, this will change when
  // we have two types of servers: UDF and data servers. Then the responses are
  // combined and the result is returned. If any underlying request fails -- we
  // return an empty response and `Internal` error as the status for the gRPC
  // status code.
  absl::StatusOr<InternalLookupResponse> GetKeyValues(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& keys) const override {
    ScopeLatencyMetricsRecorder<UdfRequestMetricsContext,
                                kShardedLookupGetKeyValuesLatencyInMicros>
        latency_recorder(request_context.GetUdfRequestMetricsContext());
    return ProcessShardedKeys(request_context, keys);
  }

  absl::StatusOr<InternalLookupResponse> GetKeyValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& keys) const override {
    ScopeLatencyMetricsRecorder<UdfRequestMetricsContext,
                                kShardedLookupGetKeyValueSetLatencyInMicros>
        latency_recorder(request_context.GetUdfRequestMetricsContext());
    InternalLookupResponse response;
    if (keys.empty()) {
      return response;
    }
    absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> key_sets;
    auto get_key_value_set_result_maybe =
        GetShardedKeyValueSet(request_context, keys);
    if (!get_key_value_set_result_maybe.ok()) {
      LogUdfRequestErrorMetric(request_context.GetUdfRequestMetricsContext(),
                               kShardedGetKeyValueSetKeySetRetrievalFailure);
      return get_key_value_set_result_maybe.status();
    }
    key_sets = *std::move(get_key_value_set_result_maybe);

    for (const auto& key : keys) {
      SingleLookupResult result;
      const auto key_iter = key_sets.find(key);
      if (key_iter == key_sets.end()) {
        auto status = result.mutable_status();
        status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
        LogUdfRequestErrorMetric(request_context.GetUdfRequestMetricsContext(),
                                 kShardedGetKeyValueSetKeySetNotFound);
      } else {
        auto keyset_values = result.mutable_keyset_values();
        keyset_values->mutable_values()->Add(key_iter->second.begin(),
                                             key_iter->second.end());
      }
      (*response.mutable_kv_pairs())[key] = std::move(result);
    }
    return response;
  }

  absl::StatusOr<InternalRunQueryResponse> RunQuery(
      const RequestContext& request_context, std::string query) const override {
    ScopeLatencyMetricsRecorder<UdfRequestMetricsContext,
                                kShardedLookupRunQueryLatencyInMicros>
        latency_recorder(request_context.GetUdfRequestMetricsContext());
    InternalRunQueryResponse response;
    if (query.empty()) {
      LogUdfRequestErrorMetric(request_context.GetUdfRequestMetricsContext(),
                               kShardedRunQueryEmptyQuery);
      return response;
    }

    absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> keysets;
    kv_server::Driver driver([&keysets, this,
                              &request_context](std::string_view key) {
      const auto key_iter = keysets.find(key);
      if (key_iter == keysets.end()) {
        VLOG(8) << "Driver can't find " << key << "key_set. Returning empty.";
        LogUdfRequestErrorMetric(request_context.GetUdfRequestMetricsContext(),
                                 kShardedRunQueryMissingKeySet);
        absl::flat_hash_set<std::string_view> set;
        return set;
      } else {
        absl::flat_hash_set<std::string_view> set(key_iter->second.begin(),
                                                  key_iter->second.end());
        return set;
      }
    });
    std::istringstream stream(query);
    kv_server::Scanner scanner(stream);
    kv_server::Parser parse(driver, scanner);
    int parse_result = parse();
    if (parse_result) {
      LogUdfRequestErrorMetric(request_context.GetUdfRequestMetricsContext(),
                               kShardedRunQueryParsingFailure);
      return absl::InvalidArgumentError("Parsing failure.");
    }
    auto get_key_value_set_result_maybe =
        GetShardedKeyValueSet(request_context, driver.GetRootNode()->Keys());
    if (!get_key_value_set_result_maybe.ok()) {
      LogUdfRequestErrorMetric(request_context.GetUdfRequestMetricsContext(),
                               kShardedRunQueryKeySetRetrievalFailure);
      return get_key_value_set_result_maybe.status();
    }
    keysets = std::move(*get_key_value_set_result_maybe);
    auto result = driver.GetResult();
    if (!result.ok()) {
      LogUdfRequestErrorMetric(request_context.GetUdfRequestMetricsContext(),
                               kShardedRunQueryFailure);
      return result.status();
    }
    VLOG(8) << "Driver results for query " << query;
    for (const auto& value : *result) {
      VLOG(8) << "Value: " << value << "\n";
    }

    response.mutable_elements()->Assign(result->begin(), result->end());
    return response;
  }

 private:
  // Keeps sharded keys and assosiated metdata.
  struct ShardLookupInput {
    // Keys that are being looked up.
    std::vector<std::string_view> keys;
    // A serialized `InternalLookupRequest` with the corresponding keys
    // from `keys`.
    std::string serialized_request;
    // Identifies by how many chars `keys` should be padded, so that
    // all requests add up to the same length.
    int32_t padding;
  };

  std::vector<ShardLookupInput> BucketKeys(
      const absl::flat_hash_set<std::string_view>& keys) const {
    ShardLookupInput sli;
    std::vector<ShardLookupInput> lookup_inputs(num_shards_, sli);
    for (const auto key : keys) {
      auto sharding_result = key_sharder_.GetShardNumForKey(key, num_shards_);
      VLOG(9) << "key: " << key
              << ", shard number: " << sharding_result.shard_num
              << ", sharding_key (if regex is present): "
              << sharding_result.sharding_key;
      lookup_inputs[sharding_result.shard_num].keys.emplace_back(key);
    }
    return lookup_inputs;
  }

  void SerializeShardedRequests(std::vector<ShardLookupInput>& lookup_inputs,
                                bool lookup_sets) const {
    for (auto& lookup_input : lookup_inputs) {
      InternalLookupRequest request;
      request.mutable_keys()->Assign(lookup_input.keys.begin(),
                                     lookup_input.keys.end());
      request.set_lookup_sets(lookup_sets);
      lookup_input.serialized_request = request.SerializeAsString();
    }
  }

  void ComputePadding(std::vector<ShardLookupInput>& lookup_inputs) const {
    int32_t max_length = 0;
    for (const auto& lookup_input : lookup_inputs) {
      max_length =
          std::max(max_length, int32_t(lookup_input.serialized_request.size()));
    }
    for (auto& lookup_input : lookup_inputs) {
      lookup_input.padding =
          max_length - lookup_input.serialized_request.size();
    }
  }

  std::vector<ShardLookupInput> ShardKeys(
      const absl::flat_hash_set<std::string_view>& keys,
      bool lookup_sets) const {
    auto lookup_inputs = BucketKeys(keys);
    SerializeShardedRequests(lookup_inputs, lookup_sets);
    ComputePadding(lookup_inputs);
    return lookup_inputs;
  }

  absl::StatusOr<
      std::vector<std::future<absl::StatusOr<InternalLookupResponse>>>>
  GetLookupFutures(const RequestContext& request_context,
                   const std::vector<ShardLookupInput>& shard_lookup_inputs,
                   std::function<absl::StatusOr<InternalLookupResponse>(
                       const std::vector<std::string_view>& key_list)>
                       get_local_future) const {
    std::vector<std::future<absl::StatusOr<InternalLookupResponse>>> responses;
    for (int shard_num = 0; shard_num < num_shards_; shard_num++) {
      auto& shard_lookup_input = shard_lookup_inputs[shard_num];
      LogIfError(request_context.GetUdfRequestMetricsContext()
                     .AccumulateMetric<kShardedLookupKeyCountByShard>(
                         (int)shard_lookup_input.keys.size(),
                         std::to_string(shard_num)));
      if (shard_num == current_shard_num_) {
        // Eventually this whole branch will go away.
        responses.push_back(std::async(std::launch::async, get_local_future,
                                       std::ref(shard_lookup_input.keys)));
      } else {
        const auto client = shard_manager_.Get(shard_num);
        if (client == nullptr) {
          LogUdfRequestErrorMetric(
              request_context.GetUdfRequestMetricsContext(),
              kLookupClientMissing);
          return absl::InternalError("Internal lookup client is unavailable.");
        }
        responses.push_back(std::async(
            std::launch::async,
            [client, &request_context](std::string_view serialized_request,
                                       int32_t padding) {
              return client->GetValues(request_context, serialized_request,
                                       padding);
            },
            shard_lookup_input.serialized_request, shard_lookup_input.padding));
      }
    }
    return responses;
  }

  absl::StatusOr<InternalLookupResponse> GetLocalValues(
      const RequestContext& request_context,
      const std::vector<std::string_view>& key_list) const {
    InternalLookupResponse response;
    absl::flat_hash_set<std::string_view> keys(key_list.begin(),
                                               key_list.end());
    return local_lookup_.GetKeyValues(request_context, keys);
  }

  absl::StatusOr<InternalLookupResponse> GetLocalKeyValuesSet(
      const RequestContext& request_context,
      const std::vector<std::string_view>& key_list) const {
    if (key_list.empty()) {
      InternalLookupResponse response;
      return response;
    }

    // We have this conversion, because of the inconsistency how we look up
    // keys in Cache -- GetKeyValuePairs vs GetKeyValueSet. GetKeyValuePairs
    // should be refactored to flat_hash_set, and then this can be fixed.
    // Additionally, this whole local branch will go away once we have a
    // a sepration between UDF and Data servers.
    absl::flat_hash_set<std::string_view> key_list_set(key_list.begin(),
                                                       key_list.end());
    return local_lookup_.GetKeyValueSet(request_context, key_list_set);
  }

  absl::StatusOr<InternalLookupResponse> ProcessShardedKeys(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& keys) const {
    InternalLookupResponse response;
    if (keys.empty()) {
      return response;
    }
    const auto shard_lookup_inputs = ShardKeys(keys, false);
    auto responses =
        GetLookupFutures(request_context, shard_lookup_inputs,
                         [this, &request_context](
                             const std::vector<std::string_view>& key_list) {
                           return GetLocalValues(request_context, key_list);
                         });
    if (!responses.ok()) {
      return responses.status();
    }
    // process responses
    for (int shard_num = 0; shard_num < num_shards_; shard_num++) {
      auto& shard_lookup_input = shard_lookup_inputs[shard_num];
      auto result = (*responses)[shard_num].get();
      if (!result.ok()) {
        // mark all keys as internal failure
        LogUdfRequestErrorMetric(request_context.GetUdfRequestMetricsContext(),
                                 kShardedKeyValueRequestFailure);
        SetRequestFailed(shard_lookup_input.keys, response);
        continue;
      }
      auto kv_pairs = result->mutable_kv_pairs();
      UpdateResponse(shard_lookup_input.keys, *kv_pairs, response);
    }
    return response;
  }

  void CollectKeySets(
      const RequestContext& request_context,
      absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>&
          key_sets,
      InternalLookupResponse& keysets_lookup_response) const {
    for (auto& [key, keyset_lookup_result] :
         (*(keysets_lookup_response.mutable_kv_pairs()))) {
      switch (keyset_lookup_result.single_lookup_result_case()) {
        case SingleLookupResult::kStatusFieldNumber:
          // this means it wasn't found, no need to insert an empty set.
          break;
        case SingleLookupResult::kKeysetValuesFieldNumber:
          absl::flat_hash_set<std::string> value_set;
          for (auto& v : keyset_lookup_result.keyset_values().values()) {
            VLOG(8) << "keyset name: " << key << " value: " << v;
            value_set.emplace(std::move(v));
          }
          auto [_, inserted] =
              key_sets.insert_or_assign(key, std::move(value_set));
          if (!inserted) {
            LogUdfRequestErrorMetric(
                request_context.GetUdfRequestMetricsContext(),
                kShardedKeyCollisionOnKeySetCollection);
            LOG(ERROR) << "Key collision, when collecting results from shards: "
                       << key;
          }
          break;
      }
    }
  }

  absl::StatusOr<
      absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>>
  GetShardedKeyValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const {
    const auto shard_lookup_inputs = ShardKeys(key_set, true);
    auto responses = GetLookupFutures(
        request_context, shard_lookup_inputs,
        [this,
         &request_context](const std::vector<std::string_view>& key_list) {
          return GetLocalKeyValuesSet(request_context, key_list);
        });
    if (!responses.ok()) {
      LogUdfRequestErrorMetric(request_context.GetUdfRequestMetricsContext(),
                               kLookupClientMissing);
      return responses.status();
    }
    // process responses
    absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> key_sets;
    for (int shard_num = 0; shard_num < num_shards_; shard_num++) {
      auto& shard_lookup_input = shard_lookup_inputs[shard_num];
      auto result = (*responses)[shard_num].get();
      if (!result.ok()) {
        LogUdfRequestErrorMetric(request_context.GetUdfRequestMetricsContext(),
                                 kShardedKeyValueSetRequestFailure);
        return result.status();
      }
      CollectKeySets(request_context, key_sets, *result);
    }
    return key_sets;
  }

  const Lookup& local_lookup_;
  const int32_t num_shards_;
  const int32_t current_shard_num_;
  const std::string hashing_seed_;
  const ShardManager& shard_manager_;
  KeySharder key_sharder_;
};

}  // namespace

std::unique_ptr<Lookup> CreateShardedLookup(const Lookup& local_lookup,
                                            const int32_t num_shards,
                                            const int32_t current_shard_num,
                                            const ShardManager& shard_manager,
                                            KeySharder key_sharder) {
  return std::make_unique<ShardedLookup>(local_lookup, num_shards,
                                         current_shard_num, shard_manager,
                                         std::move(key_sharder));
}

}  // namespace kv_server
