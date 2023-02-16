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

#include "components/data_server/server/key_value_service_v2_impl.h"

#include <grpcpp/grpcpp.h>

#include "components/telemetry/metrics_recorder.h"
#include "components/telemetry/telemetry.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"

constexpr char* GET_VALUES_V2_SPAN = "GetValuesv2";

namespace kv_server {
namespace {

using grpc::CallbackServerContext;
using v2::GetValuesRequest;
using v2::KeyValueService;

template <typename RequestT, typename ResponseT>
using HandlerFunctionT = grpc::Status (GetValuesV2Handler::*)(const RequestT&,
                                                              ResponseT*) const;

template <typename RequestT, typename ResponseT>
grpc::ServerUnaryReactor* HandleRequest(
    CallbackServerContext* context, const RequestT* request,
    ResponseT* response, const GetValuesV2Handler& handler,
    HandlerFunctionT<RequestT, ResponseT> handler_function) {
  auto span = GetTracer()->StartSpan(GET_VALUES_V2_SPAN);
  auto scope = opentelemetry::trace::Scope(span);

  grpc::Status status = (handler.*handler_function)(*request, response);

  auto* reactor = context->DefaultReactor();
  reactor->Finish(status);
  return reactor;
}

}  // namespace

grpc::ServerUnaryReactor* KeyValueServiceV2Impl::GetValues(
    CallbackServerContext* context, const GetValuesRequest* request,
    google::api::HttpBody* response) {
  return HandleRequest(context, request, response, handler_,
                       &GetValuesV2Handler::GetValues);
}

grpc::ServerUnaryReactor* KeyValueServiceV2Impl::BinaryHttpGetValues(
    CallbackServerContext* context,
    const v2::BinaryHttpGetValuesRequest* request,
    google::api::HttpBody* response) {
  return HandleRequest(context, request, response, handler_,
                       &GetValuesV2Handler::BinaryHttpGetValues);
}

grpc::ServerUnaryReactor* KeyValueServiceV2Impl::ObliviousGetValues(
    CallbackServerContext* context,
    const v2::ObliviousGetValuesRequest* request,
    google::api::HttpBody* response) {
  return HandleRequest(context, request, response, handler_,
                       &GetValuesV2Handler::ObliviousGetValues);
}

}  // namespace kv_server
