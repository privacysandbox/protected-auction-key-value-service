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

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "components/data_server/request_handler/ohttp_server_encryptor.h"
#include "components/internal_server/lookup.grpc.pb.h"
#include "components/internal_server/lookup.h"
#include "components/internal_server/string_padder.h"
#include "google/protobuf/message.h"
#include "grpcpp/grpcpp.h"
#include "src/cpp/telemetry/telemetry.h"

namespace kv_server {
using google::protobuf::RepeatedPtrField;
using privacy_sandbox::server_common::ScopeLatencyRecorder;

using grpc::StatusCode;
constexpr char kKeySetNotFound[] = "KeysetNotFound";
constexpr char kDecryptionError[] = "DecryptionError";
constexpr char kUnpaddingError[] = "UnpaddingError";
constexpr char kEncryptionError[] = "EncryptionError";
constexpr char kDeserializationError[] = "DeserializationError";
constexpr char kRunQueryError[] = "RunQueryError";
constexpr char kSecureLookup[] = "SecureLookup";

grpc::Status LookupServiceImpl::ToInternalGrpcStatus(
    const absl::Status& status, const char* eventName) const {
  metrics_recorder_.IncrementEventCounter(eventName);
  return grpc::Status(StatusCode::INTERNAL,
                      absl::StrCat(status.code(), " : ", status.message()));
}

void LookupServiceImpl::ProcessKeys(const RequestContext& request_context,
                                    const RepeatedPtrField<std::string>& keys,
                                    InternalLookupResponse& response) const {
  if (keys.empty()) return;
  absl::flat_hash_set<std::string_view> key_set;
  for (const auto& key : keys) {
    key_set.insert(key);
  }
  auto lookup_result = lookup_.GetKeyValues(request_context, key_set);
  if (lookup_result.ok()) {
    response = *std::move(lookup_result);
  }
}

void LookupServiceImpl::ProcessKeysetKeys(
    const RequestContext& request_context,
    const RepeatedPtrField<std::string>& keys,
    InternalLookupResponse& response) const {
  if (keys.empty()) return;
  absl::flat_hash_set<std::string_view> key_list;
  for (const auto& key : keys) {
    key_list.insert(key);
  }
  auto key_value_set_result = lookup_.GetKeyValueSet(request_context, key_list);
  if (key_value_set_result.ok()) {
    response = *std::move(key_value_set_result);
  }
}

grpc::Status LookupServiceImpl::InternalLookup(
    grpc::ServerContext* context, const InternalLookupRequest* request,
    InternalLookupResponse* response) {
  auto scope_metrics_context = std::make_unique<ScopeMetricsContext>();
  RequestContext request_context(*scope_metrics_context);
  if (context->IsCancelled()) {
    return grpc::Status(grpc::StatusCode::CANCELLED,
                        "Deadline exceeded or client cancelled, abandoning.");
  }
  ProcessKeys(request_context, request->keys(), *response);
  return grpc::Status::OK;
}

grpc::Status LookupServiceImpl::SecureLookup(
    grpc::ServerContext* context,
    const SecureLookupRequest* secure_lookup_request,
    SecureLookupResponse* secure_response) {
  ScopeLatencyRecorder latency_recorder(std::string(kSecureLookup),
                                        metrics_recorder_);
  auto scope_metrics_context = std::make_unique<ScopeMetricsContext>();
  RequestContext request_context(*scope_metrics_context);
  if (context->IsCancelled()) {
    return grpc::Status(grpc::StatusCode::CANCELLED,
                        "Deadline exceeded or client cancelled, abandoning.");
  }
  VLOG(9) << "SecureLookup incoming";

  OhttpServerEncryptor encryptor(key_fetcher_manager_);
  auto padded_serialized_request_maybe =
      encryptor.DecryptRequest(secure_lookup_request->ohttp_request());
  if (!padded_serialized_request_maybe.ok()) {
    return ToInternalGrpcStatus(padded_serialized_request_maybe.status(),
                                kDecryptionError);
  }

  VLOG(9) << "SecureLookup decrypted";
  auto serialized_request_maybe =
      kv_server::Unpad(*padded_serialized_request_maybe);
  if (!serialized_request_maybe.ok()) {
    metrics_recorder_.IncrementEventCounter(kDeserializationError);
    return ToInternalGrpcStatus(serialized_request_maybe.status(),
                                kUnpaddingError);
  }

  VLOG(9) << "SecureLookup unpadded";
  InternalLookupRequest request;
  if (!request.ParseFromString(*serialized_request_maybe)) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Failed parsing incoming request");
  }

  auto payload_to_encrypt =
      GetPayload(request_context, request.lookup_sets(), request.keys());
  if (payload_to_encrypt.empty()) {
    // we cannot encrypt an empty payload. Note, that soon we will add logic
    // to pad responses, so this branch will never be hit.
    return grpc::Status::OK;
  }
  auto encrypted_response_payload =
      encryptor.EncryptResponse(payload_to_encrypt);
  if (!encrypted_response_payload.ok()) {
    return ToInternalGrpcStatus(encrypted_response_payload.status(),
                                kEncryptionError);
  }
  secure_response->set_ohttp_response(*encrypted_response_payload);
  return grpc::Status::OK;
}

std::string LookupServiceImpl::GetPayload(
    const RequestContext& request_context, const bool lookup_sets,
    const RepeatedPtrField<std::string>& keys) const {
  InternalLookupResponse response;
  if (lookup_sets) {
    ProcessKeysetKeys(request_context, keys, response);
  } else {
    ProcessKeys(request_context, keys, response);
  }
  return response.SerializeAsString();
}

grpc::Status LookupServiceImpl::InternalRunQuery(
    grpc::ServerContext* context, const InternalRunQueryRequest* request,
    InternalRunQueryResponse* response) {
  auto scope_metrics_context = std::make_unique<ScopeMetricsContext>();
  RequestContext request_context(*scope_metrics_context);
  if (context->IsCancelled()) {
    return grpc::Status(grpc::StatusCode::CANCELLED,
                        "Deadline exceeded or client cancelled, abandoning.");
  }
  const auto process_result =
      lookup_.RunQuery(request_context, request->query());
  if (!process_result.ok()) {
    return ToInternalGrpcStatus(process_result.status(), kRunQueryError);
  }
  *response = *std::move(process_result);
  return grpc::Status::OK;
}

}  // namespace kv_server
