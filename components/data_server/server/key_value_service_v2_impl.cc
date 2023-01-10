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

#include "public/query/v2/get_values_v2.grpc.pb.h"

namespace kv_server {

using grpc::CallbackServerContext;
using v2::GetValuesRequest;
using v2::KeyValueService;

grpc::ServerUnaryReactor* KeyValueServiceV2Impl::GetValues(
    CallbackServerContext* context, const GetValuesRequest* request,
    google::api::HttpBody* response) {
  grpc::Status status = handler_.GetValues(*request, response);

  auto* reactor = context->DefaultReactor();
  reactor->Finish(status);
  return reactor;
}

}  // namespace kv_server
