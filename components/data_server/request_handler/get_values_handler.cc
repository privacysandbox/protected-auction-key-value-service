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

#include <string>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "absl/strings/str_split.h"
#include "components/data_server/cache/cache.h"
#include "glog/logging.h"
#include "public/base_types.pb.h"
#include "public/constants.h"
#include "public/query/get_values.grpc.pb.h"
#include "src/google/protobuf/message.h"
#include "src/google/protobuf/struct.pb.h"

namespace fledge::kv_server {
namespace {
using fledge::kv_server::v1::GetValuesRequest;
using fledge::kv_server::v1::GetValuesResponse;
using fledge::kv_server::v1::KeyValueService;
using google::protobuf::RepeatedPtrField;
using google::protobuf::Struct;
using google::protobuf::Value;
using grpc::StatusCode;

grpc::Status ValidateDSPRequest(const GetValuesRequest& request) {
  if (request.keys().empty()) {
    return grpc::Status(StatusCode::INVALID_ARGUMENT, "",
                        "Missing field 'keys'");
  }
  if (!request.ad_component_render_urls().empty()) {
    return grpc::Status(StatusCode::INVALID_ARGUMENT, "",
                        "Invalid field 'adComponentRenderUrls'");
  }
  if (!request.render_urls().empty()) {
    return grpc::Status(StatusCode::INVALID_ARGUMENT, "",
                        "Invalid field 'renderUrls'");
  }
  return grpc::Status();
}

grpc::Status ValidateSSPRequest(const GetValuesRequest& request) {
  if (request.render_urls().empty()) {
    return grpc::Status(StatusCode::INVALID_ARGUMENT, "",
                        "Missing field 'renderUrls'");
  }
  if (!request.keys().empty()) {
    return grpc::Status(StatusCode::INVALID_ARGUMENT, "",
                        "Invalid field 'keys'");
  }
  if (!request.subkey().empty()) {
    return grpc::Status(StatusCode::INVALID_ARGUMENT, "",
                        "Invalid field 'subkey'");
  }
  return grpc::Status();
}

void AddNamespaceKeys(const RepeatedPtrField<std::string>& keys,
                      std::string_view subkey,
                      std::vector<Cache::FullyQualifiedKey>& full_key_list) {
  for (const auto& key : keys) {
    for (absl::string_view individual_key :
         absl::StrSplit(key, kQueryArgDelimiter)) {
      Cache::FullyQualifiedKey full_key;
      full_key.key = individual_key;
      full_key.subkey = subkey;
      full_key_list.emplace_back(full_key);
    }
  }
}

void ProcessNamespace(const RepeatedPtrField<std::string>& keys,
                      std::string_view subkey, const Cache& cache,
                      Struct& result_struct) {
  if (keys.empty()) return;
  std::vector<Cache::FullyQualifiedKey> full_key_list;
  AddNamespaceKeys(keys, subkey, full_key_list);

  auto kv_pairs = cache.GetKeyValuePairs(full_key_list);

  for (auto&& [k, v] : std::move(kv_pairs)) {
    (*result_struct.mutable_fields())[std::move(k.key)].set_string_value(
        std::move(v));
  }
}

}  // namespace

grpc::Status GetValuesHandler::GetValues(const GetValuesRequest& request,
                                         GetValuesResponse* response) const {
  grpc::Status status = ValidateRequest(request);
  if (!status.ok()) {
    return status;
  }

  std::string_view subkey = request.subkey();
  if (subkey.empty() && !request.hostname().empty()) {
    subkey = request.hostname();
  }
  VLOG(5) << "Processing kv_internal for " << request.DebugString();
  if (!request.kv_internal().empty()) {
    ProcessNamespace(request.kv_internal(), request.subkey(),
                     sharded_cache_.GetCacheShard(KeyNamespace::KV_INTERNAL),
                     *response->mutable_kv_internal());
  }
  if (dsp_mode_) {
    VLOG(5) << "Processing keys for " << request.DebugString();
    ProcessNamespace(request.keys(), request.subkey(),
                     sharded_cache_.GetCacheShard(KeyNamespace::KEYS),
                     *response->mutable_keys());
  } else {
    VLOG(5) << "Processing ssp for " << request.DebugString();
    ProcessNamespace(request.render_urls(), request.subkey(),
                     sharded_cache_.GetCacheShard(KeyNamespace::RENDER_URLS),
                     *response->mutable_render_urls());
    ProcessNamespace(
        request.ad_component_render_urls(), request.subkey(),
        sharded_cache_.GetCacheShard(KeyNamespace::AD_COMPONENT_RENDER_URLS),
        *response->mutable_ad_component_render_urls());
  }

  return grpc::Status::OK;
}

grpc::Status GetValuesHandler::ValidateRequest(
    const GetValuesRequest& request) const {
  if (!request.kv_internal().empty()) {
    // This is an internal request.
    return grpc::Status();
  }
  return dsp_mode_ ? ValidateDSPRequest(request) : ValidateSSPRequest(request);
}

}  // namespace fledge::kv_server
