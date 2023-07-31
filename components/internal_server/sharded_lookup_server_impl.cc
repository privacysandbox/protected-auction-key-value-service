// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/internal_server/sharded_lookup_server_impl.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/get_key_value_set_result.h"
#include "components/internal_server/lookup.grpc.pb.h"
#include "components/query/driver.h"
#include "components/query/scanner.h"
#include "glog/logging.h"
#include "google/protobuf/message.h"
#include "grpcpp/grpcpp.h"
#include "src/cpp/telemetry/telemetry.h"

namespace kv_server {

constexpr char kShardedLookupServerSpan[] = "ShardedLookupServerHandler";
constexpr char* kShardedLookupGrpcFailure = "ShardedLookupGrpcFailure";

using google::protobuf::RepeatedPtrField;

namespace {
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
}

absl::flat_hash_set<std::string_view> GetRequestKeys(
    const InternalLookupRequest* request) {
  absl::flat_hash_set<std::string_view> keys;
  for (auto it = request->keys().begin(); it != request->keys().end(); it++) {
    keys.emplace(*it);
  }
  return keys;
}

void CollectKeySets(
    absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>&
        key_sets,
    InternalLookupResponse& keysets_lookup_response) {
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
          LOG(ERROR) << "Key collision, when collecting results from shards: "
                     << key;
        }
        break;
    }
  }
}

grpc::Status ToInternalGrpcStatus(const absl::Status& status) {
  return grpc::Status(grpc::StatusCode::INTERNAL,
                      absl::StrCat(status.code(), " : ", status.message()));
}
}  // namespace

absl::StatusOr<std::vector<std::future<absl::StatusOr<InternalLookupResponse>>>>
ShardedLookupServiceImpl::GetLookupFutures(
    const std::vector<ShardedLookupServiceImpl::ShardLookupInput>&
        shard_lookup_inputs,
    std::function<absl::StatusOr<InternalLookupResponse>(
        const std::vector<std::string_view>& key_list)>
        get_local_future) const {
  std::vector<std::future<absl::StatusOr<InternalLookupResponse>>> responses;
  for (int shard_num = 0; shard_num < num_shards_; shard_num++) {
    auto& shard_lookup_input = shard_lookup_inputs[shard_num];
    if (shard_num == current_shard_num_) {
      // Eventually this whole branch will go away.
      responses.push_back(std::async(std::launch::async, get_local_future,
                                     std::ref(shard_lookup_input.keys)));
    } else {
      const auto client = shard_manager_.Get(shard_num);
      if (client == nullptr) {
        return absl::InternalError("Internal lookup client is unavailable.");
      }
      responses.push_back(std::async(
          std::launch::async,
          [client](std::string_view serialized_request, int32_t padding) {
            return client->GetValues(serialized_request, padding);
          },
          shard_lookup_input.serialized_request, shard_lookup_input.padding));
    }
  }
  return responses;
}

absl::Status ShardedLookupServiceImpl::ProcessShardedKeys(
    const absl::flat_hash_set<std::string_view>& keys,
    InternalLookupResponse& response) const {
  if (keys.empty()) {
    return absl::OkStatus();
  }
  const auto shard_lookup_inputs = ShardKeys(keys, false);
  auto responses = GetLookupFutures(
      shard_lookup_inputs,
      [this](const std::vector<std::string_view>& key_list) {
        return ShardedLookupServiceImpl::GetLocalValues(key_list);
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
      SetRequestFailed(shard_lookup_input.keys, response);
      continue;
    }
    auto kv_pairs = result->mutable_kv_pairs();
    UpdateResponse(shard_lookup_input.keys, *kv_pairs, response);
  }
  return absl::OkStatus();
}

absl::StatusOr<InternalLookupResponse> ShardedLookupServiceImpl::GetLocalValues(
    const std::vector<std::string_view>& key_list) const {
  InternalLookupResponse response;
  if (key_list.empty()) {
    return response;
  }
  for (auto& [key, value] : cache_.GetKeyValuePairs(key_list)) {
    SingleLookupResult lookup_result;
    lookup_result.set_value(std::move(value));
    (*response.mutable_kv_pairs())[key] = std::move(lookup_result);
  }
  return response;
}

absl::StatusOr<InternalLookupResponse>
ShardedLookupServiceImpl::GetLocalKeyValuesSet(
    const std::vector<std::string_view>& key_list) const {
  InternalLookupResponse response;
  if (key_list.empty()) {
    return response;
  }

  // We have this conversion, because of the inconsistency how we look up
  // keys in Cache -- GetKeyValuePairs vs GetKeyValueSet. GetKeyValuePairs
  // should be refactored to flat_hash_set, and then this can be fixed.
  // Additionally, this whole local branch will go away once we have a
  // a sepration between UDF and Data servers.
  absl::flat_hash_set<std::string_view> key_list_set(key_list.begin(),
                                                     key_list.end());
  std::unique_ptr<GetKeyValueSetResult> key_value_set_result =
      cache_.GetKeyValueSet(key_list_set);
  for (const auto& key : key_list) {
    SingleLookupResult result;
    const auto value_set = key_value_set_result->GetValueSet(key);
    if (value_set.empty()) {
      auto status = result.mutable_status();
      status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
    } else {
      auto keyset_values = result.mutable_keyset_values();
      keyset_values->mutable_values()->Add(value_set.begin(), value_set.end());
    }
    (*response.mutable_kv_pairs())[key] = std::move(result);
  }
  return response;
}

std::vector<ShardedLookupServiceImpl::ShardLookupInput>
ShardedLookupServiceImpl::BucketKeys(
    const absl::flat_hash_set<std::string_view>& keys) const {
  ShardLookupInput sli;
  std::vector<ShardLookupInput> lookup_inputs(num_shards_, sli);
  for (const auto& key : keys) {
    int32_t shard_num = hash_function_(key, num_shards_);
    VLOG(9) << "key: " << key << ", shard number: " << shard_num;
    lookup_inputs[shard_num].keys.emplace_back(key);
  }
  return lookup_inputs;
}

void ShardedLookupServiceImpl::SerializeShardedRequests(
    std::vector<ShardedLookupServiceImpl::ShardLookupInput>& lookup_inputs,
    bool lookup_sets) const {
  for (auto& lookup_input : lookup_inputs) {
    InternalLookupRequest request;
    request.mutable_keys()->Assign(lookup_input.keys.begin(),
                                   lookup_input.keys.end());
    request.set_lookup_sets(lookup_sets);
    lookup_input.serialized_request = request.SerializeAsString();
  }
}

void ShardedLookupServiceImpl::ComputePadding(
    std::vector<ShardedLookupServiceImpl::ShardLookupInput>& lookup_inputs)
    const {
  int32_t max_length = 0;
  for (const auto& lookup_input : lookup_inputs) {
    max_length =
        std::max(max_length, int32_t(lookup_input.serialized_request.size()));
  }
  for (auto& lookup_input : lookup_inputs) {
    lookup_input.padding = max_length - lookup_input.serialized_request.size();
  }
}

std::vector<ShardedLookupServiceImpl::ShardLookupInput>
ShardedLookupServiceImpl::ShardKeys(
    const absl::flat_hash_set<std::string_view>& keys, bool lookup_sets) const {
  auto lookup_inputs = BucketKeys(keys);
  SerializeShardedRequests(lookup_inputs, lookup_sets);
  ComputePadding(lookup_inputs);
  return lookup_inputs;
}

grpc::Status ShardedLookupServiceImpl::InternalLookup(
    grpc::ServerContext* context, const InternalLookupRequest* request,
    InternalLookupResponse* response) {
  auto current_status = grpc::Status::OK;

  auto result = ProcessShardedKeys(GetRequestKeys(request), *response);
  if (!result.ok()) {
    metrics_recorder_.IncrementEventCounter(kShardedLookupGrpcFailure);
    current_status = grpc::Status(grpc::StatusCode::INTERNAL, "Internal error");
  }
  return current_status;
}

absl::StatusOr<
    absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>>
ShardedLookupServiceImpl::GetShardedKeyValueSet(
    const absl::flat_hash_set<std::string_view>& key_set) const {
  const auto shard_lookup_inputs = ShardKeys(key_set, true);
  auto responses = GetLookupFutures(
      shard_lookup_inputs,
      [this](const std::vector<std::string_view>& key_list) {
        return ShardedLookupServiceImpl::GetLocalKeyValuesSet(key_list);
      });
  if (!responses.ok()) {
    return responses.status();
  }
  // process responses
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> key_sets;
  for (int shard_num = 0; shard_num < num_shards_; shard_num++) {
    auto& shard_lookup_input = shard_lookup_inputs[shard_num];
    auto result = (*responses)[shard_num].get();
    if (!result.ok()) {
      return result.status();
    }
    CollectKeySets(key_sets, *result);
  }
  return key_sets;
}

grpc::Status ShardedLookupServiceImpl::InternalRunQuery(
    grpc::ServerContext* context,
    const kv_server::InternalRunQueryRequest* request,
    kv_server::InternalRunQueryResponse* response) {
  if (request->query().empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Query is empty.");
  }
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> keysets;
  kv_server::Driver driver([&keysets](std::string_view key) {
    const auto key_iter = keysets.find(key);
    if (key_iter == keysets.end()) {
      VLOG(8) << "Driver can't find " << key << "key_set. Returning empty.";
      absl::flat_hash_set<std::string_view> set;
      return set;
    } else {
      absl::flat_hash_set<std::string_view> set(key_iter->second.begin(),
                                                key_iter->second.end());
      return set;
    }
  });
  std::istringstream stream(request->query());
  kv_server::Scanner scanner(stream);
  kv_server::Parser parse(driver, scanner);
  int parse_result = parse();
  if (parse_result) {
    return ToInternalGrpcStatus(absl::InvalidArgumentError("Parsing failure."));
  }
  auto get_key_value_set_result_maybe =
      GetShardedKeyValueSet(driver.GetRootNode()->Keys());
  if (!get_key_value_set_result_maybe.ok()) {
    return ToInternalGrpcStatus(get_key_value_set_result_maybe.status());
  }
  keysets = std::move(*get_key_value_set_result_maybe);
  auto result = driver.GetResult();
  if (!result.ok()) {
    return ToInternalGrpcStatus(result.status());
  }
  VLOG(8) << "Driver results for query " << request->query();
  for (const auto& value : *result) {
    VLOG(8) << "Value: " << value << "\n";
  }
  response->mutable_elements()->Assign(result->begin(), result->end());
  return grpc::Status::OK;
}

}  // namespace kv_server
