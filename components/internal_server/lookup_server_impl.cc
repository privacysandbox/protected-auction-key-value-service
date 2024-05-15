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

#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "components/data_server/request_handler/ohttp_server_encryptor.h"
#include "components/internal_server/lookup.h"
#include "components/internal_server/string_padder.h"
#include "google/protobuf/message.h"
#include "grpcpp/grpcpp.h"

namespace kv_server {

using google::protobuf::RepeatedPtrField;
using grpc::StatusCode;

grpc::Status LookupServiceImpl::ToInternalGrpcStatus(
    InternalLookupMetricsContext& metrics_context, const absl::Status& status,
    std::string_view error_code) const {
  LogInternalLookupRequestErrorMetric(metrics_context, error_code);
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
  RequestContext request_context;
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
  RequestContext request_context;
  LogIfError(request_context.GetInternalLookupMetricsContext()
                 .AccumulateMetric<kSecureLookupRequestCount>(1));
  ScopeLatencyMetricsRecorder<InternalLookupMetricsContext,
                              kInternalSecureLookupLatencyInMicros>
      latency_recorder(request_context.GetInternalLookupMetricsContext());
  if (context->IsCancelled()) {
    return grpc::Status(grpc::StatusCode::CANCELLED,
                        "Deadline exceeded or client cancelled, abandoning.");
  }
  PS_VLOG(9, request_context.GetPSLogContext()) << "SecureLookup incoming";

  OhttpServerEncryptor encryptor(key_fetcher_manager_);
  auto padded_serialized_request_maybe =
      encryptor.DecryptRequest(secure_lookup_request->ohttp_request(),
                               request_context.GetPSLogContext());
  if (!padded_serialized_request_maybe.ok()) {
    return ToInternalGrpcStatus(
        request_context.GetInternalLookupMetricsContext(),
        padded_serialized_request_maybe.status(), kRequestDecryptionFailure);
  }

  PS_VLOG(9, request_context.GetPSLogContext()) << "SecureLookup decrypted";
  auto serialized_request_maybe =
      kv_server::Unpad(*padded_serialized_request_maybe);
  if (!serialized_request_maybe.ok()) {
    return ToInternalGrpcStatus(
        request_context.GetInternalLookupMetricsContext(),
        serialized_request_maybe.status(), kRequestUnpaddingError);
  }

  PS_VLOG(9, request_context.GetPSLogContext()) << "SecureLookup unpadded";
  InternalLookupRequest request;
  if (!request.ParseFromString(*serialized_request_maybe)) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Failed parsing incoming request");
  }
  request_context.UpdateLogContext(request.log_context(),
                                   request.consented_debug_config());
  auto payload_to_encrypt =
      GetPayload(request_context, request.lookup_sets(), request.keys());
  if (payload_to_encrypt.empty()) {
    // we cannot encrypt an empty payload. Note, that soon we will add logic
    // to pad responses, so this branch will never be hit.
    return grpc::Status::OK;
  }
  auto encrypted_response_payload = encryptor.EncryptResponse(
      payload_to_encrypt, request_context.GetPSLogContext());
  if (!encrypted_response_payload.ok()) {
    return ToInternalGrpcStatus(
        request_context.GetInternalLookupMetricsContext(),
        encrypted_response_payload.status(), kResponseEncryptionFailure);
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
  return RunSetQuery<InternalRunQueryRequest, InternalRunQueryResponse>(
      context, request, response,
      [this](const RequestContext& request_context, std::string query) {
        return lookup_.RunQuery(request_context, query);
      });
}

grpc::Status LookupServiceImpl::InternalRunSetQueryInt(
    grpc::ServerContext* context,
    const kv_server::InternalRunSetQueryIntRequest* request,
    kv_server::InternalRunSetQueryIntResponse* response) {
  return RunSetQuery<InternalRunSetQueryIntRequest,
                     InternalRunSetQueryIntResponse>(
      context, request, response,
      [this](const RequestContext& request_context, std::string query) {
        return lookup_.RunSetQueryInt(request_context, query);
      });
}

}  // namespace kv_server
