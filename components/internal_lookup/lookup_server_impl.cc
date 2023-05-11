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

#include "components/internal_lookup/lookup_server_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "components/data_server/cache/cache.h"
#include "components/internal_lookup/lookup.grpc.pb.h"
#include "google/protobuf/message.h"
#include "grpcpp/grpcpp.h"
#include "src/cpp/telemetry/telemetry.h"

namespace kv_server {

constexpr char kInternalLookupServerSpan[] = "InternalLookupServerHandler";

using google::protobuf::RepeatedPtrField;
using privacy_sandbox::server_common::GetTracer;

void ProcessKeys(RepeatedPtrField<std::string> keys, const Cache& cache,
                 InternalLookupResponse& response) {
  if (keys.empty()) return;
  std::vector<std::string_view> key_list;
  for (const auto& key : keys) {
    key_list.emplace_back(std::move(key));
  }
  auto kv_pairs = cache.GetKeyValuePairs(key_list);

  for (const auto& key : key_list) {
    SingleLookupResult result;
    const auto key_iter = kv_pairs.find(key);
    if (key_iter == kv_pairs.end()) {
      auto status = result.mutable_status();
      status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
      status->set_message("Key not found");
    } else {
      result.set_value(std::move(key_iter->second));
    }
    (*response.mutable_kv_pairs())[key] = std::move(result);
  }
}

grpc::ServerUnaryReactor* LookupServiceImpl::InternalLookup(
    grpc::CallbackServerContext* context, const InternalLookupRequest* request,
    InternalLookupResponse* response) {
  auto span = GetTracer()->StartSpan(kInternalLookupServerSpan);
  auto scope = opentelemetry::trace::Scope(span);

  ProcessKeys(request->keys(), cache_, *response);

  auto* reactor = context->DefaultReactor();
  reactor->Finish(grpc::Status::OK);
  return reactor;
}

}  // namespace kv_server
