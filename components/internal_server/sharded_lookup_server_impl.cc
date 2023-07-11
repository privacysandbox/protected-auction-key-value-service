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
#include <future>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "components/data_server/cache/cache.h"
#include "components/internal_server/lookup.grpc.pb.h"
#include "glog/logging.h"
#include "google/protobuf/message.h"
#include "grpcpp/grpcpp.h"
#include "src/cpp/telemetry/telemetry.h"

namespace kv_server {

constexpr char kShardedLookupServerSpan[] = "ShardedLookupServerHandler";
constexpr char* kShardedLookupGrpcFailure = "ShardedLookupGrpcFailure";

using google::protobuf::RepeatedPtrField;

namespace {
template <typename T, typename K>
void UpdateResponse(const std::vector<std::string_view>& key_list, T& kv_pairs,
                    std::function<SingleLookupResult(K&&)>&& value_mapper,
                    InternalLookupResponse& response) {
  for (const auto& key : key_list) {
    const auto key_iter = kv_pairs.find(key);
    if (key_iter == kv_pairs.end()) {
      SingleLookupResult result;
      auto status = result.mutable_status();
      status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
      status->set_message("Key not found");
      (*response.mutable_kv_pairs())[key] = std::move(result);
    } else {
      (*response.mutable_kv_pairs())[key] =
          value_mapper(std::move(key_iter->second));
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

}  // namespace

absl::Status ShardedLookupServiceImpl::ProcessShardedKeys(
    const RepeatedPtrField<std::string>& keys, const Cache& cache,
    InternalLookupResponse& response) {
  if (keys.empty()) {
    return absl::OkStatus();
  }

  const auto shard_lookup_inputs = ShardKeys(keys);
  std::vector<std::future<absl::StatusOr<InternalLookupResponse>>> responses;
  for (int shard_num = 0; shard_num < num_shards_; shard_num++) {
    auto& shard_lookup_input = shard_lookup_inputs[shard_num];
    const auto& key_list = shard_lookup_input.keys;
    if (shard_num == current_shard_num_) {
      // Eventually this whole branch will go away. Meanwhile, we need the
      // following line for proper indexing order when we process responses.
      responses.emplace_back();
      if (key_list.empty()) {
        continue;
      }
      auto kv_pairs = cache.GetKeyValuePairs(key_list);
      UpdateResponse<absl::flat_hash_map<std::string, std::string>,
                     std::string>(
          key_list, kv_pairs,
          [](std::string result) {
            SingleLookupResult actresult;
            actresult.set_value(std::move(result));
            return actresult;
          },
          response);
    } else {
      auto client = shard_manager_.Get(shard_num);
      if (client == nullptr) {
        return absl::InternalError("Internal lookup client is unavailable.");
      }
      responses.push_back(std::async(
          std::launch::async, &ShardedLookupServiceImpl::GetValues, this,
          std::ref(*client), shard_lookup_input.serialized_request,
          shard_lookup_input.padding));
    }
  }
  // process responses
  for (int shard_num = 0; shard_num < num_shards_; shard_num++) {
    auto& shard_lookup_input = shard_lookup_inputs[shard_num];
    if (shard_num == current_shard_num_) {
      continue;
    }

    auto result = responses[shard_num].get();
    if (!result.ok()) {
      // mark all keys as internal failure
      SetRequestFailed(shard_lookup_input.keys, response);
      continue;
    }
    auto kv_pairs = result->mutable_kv_pairs();
    UpdateResponse<
        ::google::protobuf::Map<std::string, ::kv_server::SingleLookupResult>,
        SingleLookupResult>(
        shard_lookup_input.keys, *kv_pairs,
        [](SingleLookupResult result) { return result; }, response);
  }
  return absl::OkStatus();
}

absl::StatusOr<InternalLookupResponse> ShardedLookupServiceImpl::GetValues(
    RemoteLookupClient& client, std::string_view serialized_message,
    int32_t padding_length) {
  return client.GetValues(serialized_message, padding_length);
}

std::vector<ShardedLookupServiceImpl::ShardLookupInput>
ShardedLookupServiceImpl::BucketKeys(
    const RepeatedPtrField<std::string>& keys) const {
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
    std::vector<ShardedLookupServiceImpl::ShardLookupInput>& lookup_inputs)
    const {
  for (auto& lookup_input : lookup_inputs) {
    InternalLookupRequest request;
    request.mutable_keys()->Assign(lookup_input.keys.begin(),
                                   lookup_input.keys.end());
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
    const RepeatedPtrField<std::string>& keys) const {
  auto lookup_inputs = BucketKeys(keys);
  SerializeShardedRequests(lookup_inputs);
  ComputePadding(lookup_inputs);
  return lookup_inputs;
}

grpc::Status ShardedLookupServiceImpl::InternalLookup(
    grpc::ServerContext* context, const InternalLookupRequest* request,
    InternalLookupResponse* response) {
  auto current_status = grpc::Status::OK;
  auto result = ProcessShardedKeys(request->keys(), cache_, *response);
  if (!result.ok()) {
    metrics_recorder_.IncrementEventCounter(kShardedLookupGrpcFailure);
    current_status = grpc::Status(grpc::StatusCode::INTERNAL, "Internal error");
  }
  return current_status;
}

}  // namespace kv_server
