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

#ifndef COMPONENTS_DATA_SERVER_SERVER_KEY_VALUE_SERVICE_V2_IMPL_H_
#define COMPONENTS_DATA_SERVER_SERVER_KEY_VALUE_SERVICE_V2_IMPL_H_

#include <memory>
#include <utility>

#include "components/data_server/cache/cache.h"
#include "components/data_server/request_handler/get_values_v2_handler.h"
#include "grpcpp/grpcpp.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"
#include "src/cpp/telemetry/metrics_recorder.h"

namespace kv_server {

// Implements Key-Value service Query V2 API.
class KeyValueServiceV2Impl final
    : public v2::KeyValueService::CallbackService {
 public:
  explicit KeyValueServiceV2Impl(
      GetValuesV2Handler handler,
      privacy_sandbox::server_common::MetricsRecorder& metrics_recorder)
      : handler_(std::move(handler)), metrics_recorder_(metrics_recorder) {}

  grpc::ServerUnaryReactor* GetValues(grpc::CallbackServerContext* context,
                                      const v2::GetValuesRequest* request,
                                      google::api::HttpBody* response) override;

  grpc::ServerUnaryReactor* BinaryHttpGetValues(
      grpc::CallbackServerContext* context,
      const v2::BinaryHttpGetValuesRequest* request,
      google::api::HttpBody* response) override;

  grpc::ServerUnaryReactor* ObliviousGetValues(
      grpc::CallbackServerContext* context,
      const v2::ObliviousGetValuesRequest* request,
      google::api::HttpBody* response) override;

 private:
  const GetValuesV2Handler handler_;
  privacy_sandbox::server_common::MetricsRecorder& metrics_recorder_;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_SERVER_KEY_VALUE_SERVICE_V2_IMPL_H_
