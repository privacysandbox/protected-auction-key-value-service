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

#include "components/data_server/server/key_value_service_impl.h"

#include <string>

#include <grpcpp/grpcpp.h>

#include "components/data_server/request_handler/get_values_handler.h"
#include "public/query/get_values.grpc.pb.h"

namespace kv_server {

using google::protobuf::Struct;
using google::protobuf::Value;
using grpc::CallbackServerContext;
using v1::GetValuesRequest;
using v1::GetValuesResponse;
using v1::KeyValueService;

namespace {
// The V1 request API should have no concept of consented debugging, default
// all V1 requests to consented requests
privacy_sandbox::server_common::ConsentedDebugConfiguration
GetDefaultConsentedDebugConfigForV1Request() {
  privacy_sandbox::server_common::ConsentedDebugConfiguration config;
  config.set_is_consented(true);
  config.set_token(privacy_sandbox::server_common::log::ServerToken());
  return config;
}
}  // namespace

grpc::ServerUnaryReactor* KeyValueServiceImpl::GetValues(
    CallbackServerContext* context, const GetValuesRequest* request,
    GetValuesResponse* response) {
  privacy_sandbox::server_common::Stopwatch stopwatch;
  std::unique_ptr<RequestContextFactory> request_context_factory =
      std::make_unique<RequestContextFactory>();
  request_context_factory->UpdateLogContext(
      privacy_sandbox::server_common::LogContext(),
      GetDefaultConsentedDebugConfigForV1Request());
  grpc::Status status =
      handler_.GetValues(*request_context_factory, *request, response);
  auto* reactor = context->DefaultReactor();
  reactor->Finish(status);
  LogV1RequestCommonSafeMetrics(request, response, status, stopwatch);
  return reactor;
}

}  // namespace kv_server
