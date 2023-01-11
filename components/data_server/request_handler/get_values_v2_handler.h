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

#ifndef COMPONENTS_DATA_SERVER_REQUEST_HANDLER_GET_VALUES_V2_HANDLER_H_
#define COMPONENTS_DATA_SERVER_REQUEST_HANDLER_GET_VALUES_V2_HANDLER_H_

#include "components/data_server/cache/cache.h"
#include "grpcpp/grpcpp.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"

namespace kv_server {

// Handles GetValuesRequests.
// See the Service proto definition for details.
class GetValuesV2Handler {
 public:
  explicit GetValuesV2Handler(const ShardedCache& sharded_cache)
      : sharded_cache_(sharded_cache) {}

  grpc::Status GetValues(const v2::GetValuesRequest& request,
                         google::api::HttpBody* response) const;

 private:
  const ShardedCache& sharded_cache_;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_REQUEST_HANDLER_GET_VALUES_V2_HANDLER_H_
