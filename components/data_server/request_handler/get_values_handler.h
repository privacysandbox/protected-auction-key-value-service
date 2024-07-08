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

#ifndef COMPONENTS_DATA_SERVER_REQUEST_HANDLER_GET_VALUES_HANDLER_H_
#define COMPONENTS_DATA_SERVER_REQUEST_HANDLER_GET_VALUES_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "components/data_server/request_handler/get_values_adapter.h"
#include "grpcpp/grpcpp.h"
#include "public/query/get_values.grpc.pb.h"
#include "src/google/protobuf/struct.pb.h"

namespace kv_server {

// Handles GetValuesRequests.
// See the Service proto definition for details.
class GetValuesHandler {
 public:
  explicit GetValuesHandler(const Cache& cache, const GetValuesAdapter& adapter,
                            bool use_v2, bool add_missing_keys_v1 = true)
      : cache_(std::move(cache)),
        adapter_(std::move(adapter)),
        use_v2_(use_v2),
        add_missing_keys_v1_(add_missing_keys_v1) {}

  // TODO: Implement hostname, ad/render url lookups.
  grpc::Status GetValues(RequestContextFactory& request_context_factory,
                         const v1::GetValuesRequest& request,
                         v1::GetValuesResponse* response) const;

 private:
  const Cache& cache_;
  const GetValuesAdapter& adapter_;
  // If true, routes requests through V2 (UDF). Otherwise, calls cache.
  const bool use_v2_;
  const bool add_missing_keys_v1_;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_REQUEST_HANDLER_GET_VALUES_HANDLER_H_
