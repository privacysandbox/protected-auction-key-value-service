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

#include "absl/strings/str_split.h"
#include "components/data_server/cache/cache.h"
#include "glog/logging.h"
#include "grpcpp/grpcpp.h"
#include "infrastructure/communication/bhttp_utils.h"
#include "public/base_types.pb.h"
#include "public/constants.h"
#include "public/query/get_values.grpc.pb.h"
#include "quiche/binary_http/binary_http_message.h"
#include "src/google/protobuf/message.h"
#include "src/google/protobuf/struct.pb.h"

namespace kv_server {
namespace {
using google::protobuf::RepeatedPtrField;
using google::protobuf::Struct;
using google::protobuf::Value;
using grpc::StatusCode;
using v1::BinaryHttpGetValuesRequest;
using v1::GetValuesRequest;
using v1::GetValuesResponse;
using v1::KeyValueService;

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

grpc::Status GetValuesHandler::BinaryHttpGetValues(
    const BinaryHttpGetValuesRequest& bhttp_request,
    google::api::HttpBody* bhttp_response) const {
  VLOG(9) << "Received BinaryHttpGetValues request";
  auto maybe_request =
      DeserializeBHttpToProto<quiche::BinaryHttpRequest, GetValuesRequest>(
          bhttp_request.raw_body().data());
  if (!maybe_request.ok()) {
    VLOG(1) << "Failed to deserialize request: " << maybe_request.status();
    return grpc::Status(StatusCode::INTERNAL,
                        std::string(maybe_request.status().message()));
  }
  VLOG(3) << "BinaryHttpGetValues request: " << maybe_request->DebugString();

  GetValuesResponse response;
  const grpc::Status get_values_status = GetValues(*maybe_request, &response);
  VLOG(3) << "BinaryHttpGetValues status: " << get_values_status.error_message()
          << "response: " << response.DebugString();

  uint16_t status_code = 200;
  if (!get_values_status.ok()) {
    response.Clear();
    status_code = 500;
  }
  auto maybe_response =
      SerializeProtoToBHttp<quiche::BinaryHttpResponse, GetValuesResponse>(
          response, status_code);

  if (!maybe_response.ok()) {
    return grpc::Status(StatusCode::INTERNAL,
                        std::string(maybe_response.status().message()));
  }
  bhttp_response->set_data(std::move(*maybe_response));
  return grpc::Status::OK;
}

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

}  // namespace kv_server
