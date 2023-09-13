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
#include "components/data_server/cache/get_key_value_set_result.h"
#include "components/internal_server/lookup.grpc.pb.h"
#include "components/internal_server/lookup.h"
#include "components/query/driver.h"
#include "components/query/scanner.h"
#include "glog/logging.h"
#include "google/protobuf/message.h"
#include "grpcpp/grpcpp.h"
#include "src/cpp/telemetry/telemetry.h"

namespace kv_server {
using google::protobuf::RepeatedPtrField;
using privacy_sandbox::server_common::ScopeLatencyRecorder;

namespace {

grpc::Status ToInternalGrpcStatus(const absl::Status& status) {
  return grpc::Status(grpc::StatusCode::INTERNAL,
                      absl::StrCat(status.code(), " : ", status.message()));
}

std::vector<std::string_view> GetRequestKeys(
    const InternalLookupRequest* request) {
  std::vector<std::string_view> keys;
  for (auto& key : request->keys()) {
    keys.push_back(key);
  }
  return keys;
}

}  // namespace

grpc::Status ShardedLookupServiceImpl::InternalLookup(
    grpc::ServerContext* context, const InternalLookupRequest* request,
    InternalLookupResponse* response) {
  auto lookup_response = lookup_.GetKeyValues(GetRequestKeys(request));
  if (!lookup_response.ok()) {
    return ToInternalGrpcStatus(lookup_response.status());
  }
  *response = *std::move(lookup_response);
  return grpc::Status::OK;
}

grpc::Status ShardedLookupServiceImpl::InternalRunQuery(
    grpc::ServerContext* context,
    const kv_server::InternalRunQueryRequest* request,
    kv_server::InternalRunQueryResponse* response) {
  auto lookup_response = lookup_.RunQuery(request->query());
  if (!lookup_response.ok()) {
    return ToInternalGrpcStatus(lookup_response.status());
  }
  *response = *std::move(lookup_response);
  return grpc::Status::OK;
}

}  // namespace kv_server
