// Copyright 2022 Google LLC
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

#include "components/data_server/request_handler/get_values_v2_handler.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "glog/logging.h"
#include "grpcpp/grpcpp.h"
#include "nlohmann/json.hpp"
#include "public/base_types.pb.h"
#include "public/constants.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"

namespace kv_server {
namespace {
using grpc::StatusCode;
using v2::GetValuesRequest;
using v2::KeyValueService;

// Does a key value lookup for the keys in this group. This is a reference
// implementation.
absl::StatusOr<nlohmann::json> ProcessKeyGroup(
    const Cache& cache, const nlohmann::json& context,
    const nlohmann::json& key_group) {
  std::vector<Cache::FullyQualifiedKey> full_key_list;
  for (const auto& key : key_group["keyList"]) {
    Cache::FullyQualifiedKey full_key;
    full_key.key = std::string_view{
        key.get_ptr<const nlohmann::json::string_t*>()->c_str()};
    full_key_list.push_back(std::move(full_key));
  }
  auto kv_pairs = cache.GetKeyValuePairs(full_key_list);

  nlohmann::json group_output;
  group_output["tags"] = key_group["tags"];
  auto& key_values = group_output["keyValues"];
  for (auto&& [k, v] : std::move(kv_pairs)) {
    nlohmann::json value = {{"value", v}};
    key_values.emplace(k.key, value);
  }
  VLOG(5) << "Generated group output: " << group_output;
  return group_output;
}
// This is a reference implementation. This would be replaced by UDF
// invocations.
absl::StatusOr<nlohmann::json> ProcessPartition(
    const Cache& cache, const nlohmann::json& context,
    const nlohmann::json& partition) {
  nlohmann::json partition_output = {{"id", partition["id"]}};
  auto& group_outputs = partition_output["keyGroupOutputs"];
  for (const auto& key_group : partition["keyGroups"]) {
    const auto& tags = key_group["tags"];
    // Structured keys are not supported
    VLOG(6) << "Processing key group with tags: " << tags;
    if (std::find(tags.begin(), tags.end(), "structured") != tags.end())
      continue;
    if (auto maybe_output = ProcessKeyGroup(cache, context, key_group);
        maybe_output.ok()) {
      group_outputs.emplace_back(std::move(maybe_output).value());
    }
  }

  VLOG(5) << "Generated partition output: " << partition_output;
  return partition_output;
}

// Parses the given string into a JSON object. Does not throw JSON exceptions.
absl::StatusOr<nlohmann::json> Parse(std::string_view json_string) {
  nlohmann::json core_data_json =
      nlohmann::json::parse(json_string, nullptr, /*allow_exceptions=*/false,
                            /*ignore_comments=*/true);

  if (core_data_json.is_discarded()) {
    return absl::InvalidArgumentError("Failed to parse the request json");
  }

  return core_data_json;
}

absl::StatusOr<absl::flat_hash_map<int, nlohmann::json>>
ProcessGetValuesCoreRequest(const Cache& cache,
                            const nlohmann::json& core_data_json) {
  const nlohmann::json *partitions, *context;

  // First get the partitions and context. They will be the input to the
  // processing function
  if (auto iter = core_data_json.find("partitions");
      iter == core_data_json.end()) {
    return absl::InvalidArgumentError("Request has no partitions");
  } else {
    partitions = &iter.value();
  }
  if (auto iter = core_data_json.find("context");
      iter == core_data_json.end()) {
    return absl::InvalidArgumentError("Request has no context");
  } else {
    context = &iter.value();
  }

  // For each partition, process separately and aggregate the results in
  // compression_group_map
  absl::flat_hash_map<int, nlohmann::json> compression_group_map;
  for (const auto& partition : *partitions) {
    std::int64_t compression_group = 0;
    if (auto iter = partition.find("compressionGroup");
        iter == partition.end()) {
      return absl::InvalidArgumentError(
          "compressionGroup should be set for every partition");
    } else {
      auto compression_group_ptr =
          iter.value().get_ptr<const nlohmann::json::number_integer_t*>();
      if (!compression_group_ptr) {
        return absl::InvalidArgumentError(absl::StrCat(
            "compressionGroup should be a number. Got: ", iter.value().dump()));
      }
    }

    if (auto maybe_result = ProcessPartition(cache, *context, partition);
        maybe_result.ok()) {
      compression_group_map[compression_group]["partitions"].emplace_back(
          std::move(maybe_result).value());
    } else {
      LOG(ERROR) << "Failed to process partition: " << maybe_result.status();
    }
  }
  return compression_group_map;
}

}  // namespace

grpc::Status GetValuesV2Handler::GetValues(
    const GetValuesRequest& request, google::api::HttpBody* response) const {
  absl::StatusOr<nlohmann::json> maybe_core_request_json =
      Parse(request.raw_body().data());
  if (!maybe_core_request_json.ok()) {
    return grpc::Status(
        StatusCode::INTERNAL,
        std::string(maybe_core_request_json.status().message()));
  }

  const auto& cache = sharded_cache_.GetCacheShard(KeyNamespace::KV_INTERNAL);
  if (auto maybe_compression_group_map =
          ProcessGetValuesCoreRequest(cache, maybe_core_request_json.value());
      maybe_compression_group_map.ok()) {
    nlohmann::json response_json;
    for (auto&& [group_id, group] :
         std::move(maybe_compression_group_map).value()) {
      response_json[std::to_string(group_id)] = std::move(group);
    }
    VLOG(5) << "Uncompressed response: " << response_json.dump(1);
    response->set_data(response_json.dump());
    return grpc::Status::OK;
  } else {
    return grpc::Status(
        StatusCode::INTERNAL,
        std::string(maybe_compression_group_map.status().message()));
  }
}

}  // namespace kv_server
