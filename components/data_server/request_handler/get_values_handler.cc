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
#include "components/telemetry/metrics_recorder.h"
#include "components/telemetry/telemetry.h"
#include "glog/logging.h"
#include "grpcpp/grpcpp.h"
#include "public/constants.h"
#include "public/query/get_values.grpc.pb.h"
#include "src/google/protobuf/message.h"
#include "src/google/protobuf/struct.pb.h"

constexpr char* GET_VALUES_HANDLER_SPAN = "GetValuesHandler";
constexpr char* GET_BINARY_VALUES_HANDLER_SPAN = "BinaryGetValuesHandler";
constexpr char* CACHE_KEY_HIT = "CacheKeyHit";
constexpr char* CACHE_KEY_MISS = "CacheKeyMiss";

namespace kv_server {
namespace {
using google::protobuf::RepeatedPtrField;
using google::protobuf::Struct;
using google::protobuf::Value;
using grpc::StatusCode;
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

std::vector<std::string_view> GetKeys(
    const RepeatedPtrField<std::string>& keys) {
  std::vector<std::string_view> key_list;
  for (const auto& key : keys) {
    for (absl::string_view individual_key :
         absl::StrSplit(key, kQueryArgDelimiter)) {
      key_list.emplace_back(individual_key);
    }
  }
  return key_list;
}

void ProcessKeys(const RepeatedPtrField<std::string>& keys, const Cache& cache,
                 MetricsRecorder& metrics_recorder, Struct& result_struct) {
  if (keys.empty()) return;
  auto kv_pairs = cache.GetKeyValuePairs(GetKeys(keys));

  if (kv_pairs.empty())
    metrics_recorder.IncrementEventCounter(CACHE_KEY_MISS);
  else
    metrics_recorder.IncrementEventCounter(CACHE_KEY_HIT);

  for (auto&& [k, v] : std::move(kv_pairs)) {
    Value value_proto;
    google::protobuf::util::Status status =
        google::protobuf::util::JsonStringToMessage(v, &value_proto);
    if (status.ok()) {
      (*result_struct.mutable_fields())[std::move(k)] = value_proto;
    } else {
      // If string is not a Json string that can be parsed into Value proto,
      // simply set it as pure string value to the response.
      (*result_struct.mutable_fields())[std::move(k)].set_string_value(
          std::move(v));
    }
  }
}

}  // namespace

grpc::Status GetValuesHandler::GetValues(const GetValuesRequest& request,
                                         GetValuesResponse* response) const {
  auto span = GetTracer()->StartSpan(GET_VALUES_HANDLER_SPAN);
  auto scope = opentelemetry::trace::Scope(span);

  grpc::Status status = ValidateRequest(request);
  if (!status.ok()) {
    return status;
  }

  VLOG(5) << "Processing kv_internal for " << request.DebugString();
  if (!request.kv_internal().empty()) {
    ProcessKeys(request.kv_internal(), cache_, metrics_recorder_,
                *response->mutable_kv_internal());
  }
  if (dsp_mode_) {
    VLOG(5) << "Processing keys for " << request.DebugString();
    ProcessKeys(request.keys(), cache_, metrics_recorder_,
                *response->mutable_keys());
  } else {
    VLOG(5) << "Processing ssp for " << request.DebugString();
    ProcessKeys(request.render_urls(), cache_, metrics_recorder_,
                *response->mutable_render_urls());
    ProcessKeys(request.ad_component_render_urls(), cache_, metrics_recorder_,
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
