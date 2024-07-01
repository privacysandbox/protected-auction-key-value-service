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

#include "components/data_server/request_handler/get_values_handler.h"

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "components/data_server/request_handler/get_values_adapter.h"
#include "grpcpp/grpcpp.h"
#include "public/constants.h"
#include "public/query/get_values.grpc.pb.h"
#include "src/google/protobuf/message.h"
#include "src/google/protobuf/struct.pb.h"
#include "src/telemetry/telemetry.h"

namespace kv_server {
namespace {
using google::protobuf::RepeatedPtrField;
using google::protobuf::Struct;
using google::protobuf::Value;
using grpc::StatusCode;
using v1::GetValuesRequest;
using v1::GetValuesResponse;
using v1::KeyValueService;

absl::flat_hash_set<std::string_view> GetKeys(
    const RepeatedPtrField<std::string>& keys) {
  absl::flat_hash_set<std::string_view> key_list;
  for (const auto& key : keys) {
    for (absl::string_view individual_key :
         absl::StrSplit(key, kQueryArgDelimiter)) {
      key_list.insert(individual_key);
    }
  }
  return key_list;
}

void ProcessKeys(
    const RequestContext& request_context,
    const RepeatedPtrField<std::string>& keys, const Cache& cache,
    google::protobuf::Map<std::string, v1::V1SingleLookupResult>& result_struct,
    bool add_missing_keys_v1) {
  if (keys.empty()) return;
  auto actual_keys = GetKeys(keys);
  auto kv_pairs = cache.GetKeyValuePairs(request_context, actual_keys);
  // TODO(b/326118416): Record cache hit and miss metrics
  for (const auto& key : actual_keys) {
    v1::V1SingleLookupResult result;
    const auto key_iter = kv_pairs.find(key);
    if (key_iter == kv_pairs.end()) {
      if (add_missing_keys_v1) {
        auto status = result.mutable_status();
        status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
        status->set_message("Key not found");
        result_struct[key] = std::move(result);
      }
    } else {
      Value value_proto;
      absl::Status status = google::protobuf::util::JsonStringToMessage(
          key_iter->second, &value_proto);
      if (status.ok()) {
        *result.mutable_value() = value_proto;
      } else {
        // If string is not a Json string that can be parsed into Value
        // proto, simply set it as pure string value to the response.
        google::protobuf::Value value;
        value.set_string_value(std::move(key_iter->second));
        *result.mutable_value() = std::move(value);
      }
      result_struct[key] = std::move(result);
    }
  }
}

}  // namespace

grpc::Status GetValuesHandler::GetValues(
    RequestContextFactory& request_context_factory,
    const GetValuesRequest& request, GetValuesResponse* response) const {
  const auto& request_context = request_context_factory.Get();
  if (use_v2_) {
    PS_VLOG(5, request_context.GetPSLogContext())
        << "Using V2 adapter for " << request.DebugString();
    return adapter_.CallV2Handler(request_context_factory, request, *response);
  }
  if (!request.kv_internal().empty()) {
    PS_VLOG(5, request_context.GetPSLogContext())
        << "Processing kv_internal for " << request.DebugString();
    ProcessKeys(request_context, request.kv_internal(), cache_,
                *response->mutable_kv_internal(), add_missing_keys_v1_);
  }
  if (!request.keys().empty()) {
    PS_VLOG(5, request_context.GetPSLogContext())
        << "Processing keys for " << request.DebugString();
    ProcessKeys(request_context, request.keys(), cache_,
                *response->mutable_keys(), add_missing_keys_v1_);
  }
  if (!request.interest_group_names().empty()) {
    PS_VLOG(5, request_context.GetPSLogContext())
        << "Processing interest_group_names for " << request.DebugString();
    ProcessKeys(request_context, request.interest_group_names(), cache_,
                *response->mutable_per_interest_group_data(),
                add_missing_keys_v1_);
  }
  if (!request.render_urls().empty()) {
    PS_VLOG(5, request_context.GetPSLogContext())
        << "Processing render_urls for " << request.DebugString();
    ProcessKeys(request_context, request.render_urls(), cache_,
                *response->mutable_render_urls(), add_missing_keys_v1_);
  }
  if (!request.ad_component_render_urls().empty()) {
    PS_VLOG(5, request_context.GetPSLogContext())
        << "Processing ad_component_render_urls for " << request.DebugString();
    ProcessKeys(request_context, request.ad_component_render_urls(), cache_,
                *response->mutable_ad_component_render_urls(),
                add_missing_keys_v1_);
  }
  return grpc::Status::OK;
}

}  // namespace kv_server
