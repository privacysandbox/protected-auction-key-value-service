/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_DATA_SERVER_REQUEST_HANDLER_MOCKS_H
#define COMPONENTS_DATA_SERVER_REQUEST_HANDLER_MOCKS_H

#include "components/data_server/request_handler/get_values_adapter.h"
#include "gmock/gmock.h"
#include "grpcpp/grpcpp.h"
#include "public/query/get_values.grpc.pb.h"

namespace kv_server {

class MockGetValuesAdapter : public GetValuesAdapter {
 public:
  MOCK_METHOD((grpc::Status), CallV2Handler,
              (RequestContextFactory & request_context_factory,
               const v1::GetValuesRequest& v1_request,
               v1::GetValuesResponse& v1_response),
              (const, override));
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_REQUEST_HANDLER_MOCKS_H
