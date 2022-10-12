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

#include <grpcpp/grpcpp.h>

#include "components/data_server/request_handler/get_values_handler.h"
#include "public/query/get_values.grpc.pb.h"

namespace fledge::kv_server {

using google::protobuf::Struct;
using google::protobuf::Value;
using grpc::CallbackServerContext;
using v1::GetValuesRequest;
using v1::GetValuesResponse;
using v1::KeyValueService;

grpc::ServerUnaryReactor* KeyValueServiceImpl::GetValues(
    CallbackServerContext* context, const GetValuesRequest* request,
    GetValuesResponse* response) {
  grpc::Status status = handler_.GetValues(*request, response);

  auto* reactor = context->DefaultReactor();
  reactor->Finish(status);
  return reactor;
}

grpc::ServerUnaryReactor* KeyValueServiceImpl::BinaryHttpGetValues(
    CallbackServerContext* context,
    const v1::BinaryHttpGetValuesRequest* request,
    google::api::HttpBody* response) {
  grpc::Status status = handler_.BinaryHttpGetValues(*request, response);

  auto* reactor = context->DefaultReactor();
  reactor->Finish(status);
  return reactor;
}

}  // namespace fledge::kv_server
