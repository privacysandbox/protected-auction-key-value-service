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

#ifndef COMPONENTS_DATA_SERVER_SERVER_KEY_VALUE_SERVICE_IMPL_H_
#define COMPONENTS_DATA_SERVER_SERVER_KEY_VALUE_SERVICE_IMPL_H_

#include <memory>
#include <utility>

#include "components/data_server/cache/cache.h"
#include "components/data_server/request_handler/get_values_handler.h"
#include "grpcpp/grpcpp.h"
#include "public/query/get_values.grpc.pb.h"
#include "src/cpp/telemetry/metrics_recorder.h"

namespace kv_server {

constexpr char* kGetValuesV1Latency = "GetValuesV1Latency";

// Implements Key-Value service.
class KeyValueServiceImpl final
    : public kv_server::v1::KeyValueService::CallbackService {
 public:
  explicit KeyValueServiceImpl(
      GetValuesHandler handler,
      privacy_sandbox::server_common::MetricsRecorder& metrics_recorder)
      : handler_(std::move(handler)), metrics_recorder_(metrics_recorder) {
    metrics_recorder_.RegisterHistogram(
        kGetValuesV1Latency, "GetValues V1 service latency", "nanosecond");
  }

  grpc::ServerUnaryReactor* GetValues(
      grpc::CallbackServerContext* context,
      const kv_server::v1::GetValuesRequest* request,
      kv_server::v1::GetValuesResponse* response) override;

 private:
  GetValuesHandler handler_;
  privacy_sandbox::server_common::MetricsRecorder& metrics_recorder_;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_SERVER_KEY_VALUE_SERVICE_IMPL_H_
