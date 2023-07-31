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
#include "glog/logging.h"
#include "google/protobuf/message.h"
#include "grpcpp/grpcpp.h"
#include "src/cpp/telemetry/telemetry.h"

namespace kv_server {
using google::protobuf::RepeatedPtrField;
namespace {

using grpc::StatusCode;

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

void LookupServiceImpl::ProcessKeys(const RepeatedPtrField<std::string>& keys,
                                    InternalLookupResponse& response) const {
  if (keys.empty()) return;
  std::vector<std::string_view> key_list;
  for (const auto& key : keys) {
    key_list.emplace_back(key);
  }
  auto kv_pairs = cache_.GetKeyValuePairs(key_list);

  for (const auto& key : key_list) {
    SingleLookupResult result;
    const auto key_iter = kv_pairs.find(key);
    if (key_iter == kv_pairs.end()) {
      auto status = result.mutable_status();
      status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
    } else {
      result.set_value(std::move(key_iter->second));
    }
    (*response.mutable_kv_pairs())[key] = std::move(result);
  }
}

void LookupServiceImpl::ProcessKeysetKeys(
    const RepeatedPtrField<std::string>& keys,
    InternalLookupResponse& response) const {
  if (keys.empty()) return;
  absl::flat_hash_set<std::string_view> key_list;
  for (const auto& key : keys) {
    key_list.insert(key);
  }
  auto key_value_set_result = cache_.GetKeyValueSet(key_list);
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
}

grpc::Status LookupServiceImpl::InternalLookup(
    grpc::ServerContext* context, const InternalLookupRequest* request,
    InternalLookupResponse* response) {
  if (context->IsCancelled()) {
    return grpc::Status(grpc::StatusCode::CANCELLED,
                        "Deadline exceeded or client cancelled, abandoning.");
  }
  ProcessKeys(request->keys(), *response);
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
  VLOG(9) << "SecureLookup incoming";

  OhttpServerEncryptor encryptor(key_fetcher_manager_);
  auto padded_serialized_request_maybe =
      encryptor.DecryptRequest(secure_lookup_request->ohttp_request());
  if (!padded_serialized_request_maybe.ok()) {
    return ToInternalGrpcStatus(padded_serialized_request_maybe.status());
  }

  VLOG(9) << "SecureLookup decrypted";
  auto serialized_request_maybe =
      kv_server::Unpad(*padded_serialized_request_maybe);
  if (!serialized_request_maybe.ok()) {
    return ToInternalGrpcStatus(serialized_request_maybe.status());
  }

  VLOG(9) << "SecureLookup unpadded";
  InternalLookupRequest request;
  if (!request.ParseFromString(*serialized_request_maybe)) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Failed parsing incoming request");
  }

  auto payload_to_encrypt = GetPayload(request.lookup_sets(), request.keys());
  if (payload_to_encrypt.empty()) {
    // we cannot encrypt an empty payload. Note, that soon we will add logic
    // to pad responses, so this branch will never be hit.
    return grpc::Status::OK;
  }
  auto encrypted_response_payload =
      encryptor.EncryptResponse(payload_to_encrypt);
  if (!encrypted_response_payload.ok()) {
    return ToInternalGrpcStatus(encrypted_response_payload.status());
  }
  secure_response->set_ohttp_response(*encrypted_response_payload);
  return grpc::Status::OK;
}

std::string LookupServiceImpl::GetPayload(
    const bool lookup_sets, const RepeatedPtrField<std::string>& keys) const {
  InternalLookupResponse response;
  if (lookup_sets) {
    ProcessKeysetKeys(keys, response);
  } else {
    ProcessKeys(keys, response);
  }
  return response.SerializeAsString();
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
