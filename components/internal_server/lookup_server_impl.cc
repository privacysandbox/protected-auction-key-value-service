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

#include "components/internal_server/lookup_server_impl.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/request_handler/ohttp_server_encryptor.h"
#include "components/internal_server/lookup.grpc.pb.h"
#include "components/internal_server/string_padder.h"
#include "components/query/driver.h"
#include "components/query/scanner.h"
#include "google/protobuf/message.h"
#include "grpcpp/grpcpp.h"
#include "src/cpp/telemetry/telemetry.h"

namespace kv_server {
namespace {
using google::protobuf::RepeatedPtrField;
using grpc::StatusCode;

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

absl::Status ProcessQuery(std::string query, const Cache& cache,
                          InternalRunQueryResponse& response) {
  if (query.empty()) return absl::OkStatus();
  std::unique_ptr<GetKeyValueSetResult> get_key_value_set_result;
  kv_server::Driver driver([&get_key_value_set_result](std::string_view key) {
    return get_key_value_set_result->GetValueSet(key);
  });
  std::istringstream stream(query);
  kv_server::Scanner scanner(stream);
  kv_server::Parser parse(driver, scanner);
  int parse_result = parse();
  if (parse_result) {
    return absl::InvalidArgumentError("Parsing failure.");
  }
  get_key_value_set_result = cache.GetKeyValueSet(driver.GetRootNode()->Keys());

  auto result = driver.GetResult();
  if (!result.ok()) {
    return result.status();
  }
  response.mutable_elements()->Assign(result->begin(), result->end());
  return result.status();
}

grpc::Status ToInternalGrpcStatus(const absl::Status& status) {
  return grpc::Status(StatusCode::INTERNAL,
                      absl::StrCat(status.code(), " : ", status.message()));
}

}  // namespace

grpc::Status LookupServiceImpl::InternalLookup(
    grpc::ServerContext* context, const InternalLookupRequest* request,
    InternalLookupResponse* response) {
  if (context->IsCancelled()) {
    return grpc::Status(grpc::StatusCode::CANCELLED,
                        "Deadline exceeded or client cancelled, abandoning.");
  }
  ProcessKeys(request->keys(), cache_, *response);
  return grpc::Status::OK;
}

grpc::Status LookupServiceImpl::SecureLookup(
    grpc::ServerContext* context,
    const SecureLookupRequest* secure_lookup_request,
    SecureLookupResponse* secure_response) {
  if (context->IsCancelled()) {
    return grpc::Status(grpc::StatusCode::CANCELLED,
                        "Deadline exceeded or client cancelled, abandoning.");
  }
  OhttpServerEncryptor encryptor;
  auto padded_serialized_request_maybe =
      encryptor.DecryptRequest(secure_lookup_request->ohttp_request());
  if (!padded_serialized_request_maybe.ok()) {
    return ToInternalGrpcStatus(padded_serialized_request_maybe.status());
  }
  auto serialized_request_maybe =
      kv_server::Unpad(*padded_serialized_request_maybe);
  if (!serialized_request_maybe.ok()) {
    return ToInternalGrpcStatus(serialized_request_maybe.status());
  }
  InternalLookupRequest request;
  if (!request.ParseFromString(*serialized_request_maybe)) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Failed parsing incoming request");
  }
  InternalLookupResponse response;
  ProcessKeys(request.keys(), cache_, response);
  auto encrypted_response_payload =
      encryptor.EncryptResponse(response.SerializeAsString());
  if (!encrypted_response_payload.ok()) {
    return ToInternalGrpcStatus(encrypted_response_payload.status());
  }
  secure_response->set_ohttp_response(*encrypted_response_payload);
  return grpc::Status::OK;
}

grpc::Status LookupServiceImpl::InternalRunQuery(
    grpc::ServerContext* context, const InternalRunQueryRequest* request,
    InternalRunQueryResponse* response) {
  if (context->IsCancelled()) {
    return grpc::Status(grpc::StatusCode::CANCELLED,
                        "Deadline exceeded or client cancelled, abandoning.");
  }
  const auto process_result = ProcessQuery(request->query(), cache_, *response);
  if (!process_result.ok()) {
    return ToInternalGrpcStatus(process_result);
  }
  return grpc::Status::OK;
}

}  // namespace kv_server
