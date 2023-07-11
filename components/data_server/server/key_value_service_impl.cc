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
#include "src/cpp/telemetry/metrics_recorder.h"
#include "src/cpp/telemetry/telemetry.h"

constexpr char* kGetValuesSuccess = "GetValuesSuccess";

namespace kv_server {

using google::protobuf::Struct;
using google::protobuf::Value;
using grpc::CallbackServerContext;
using privacy_sandbox::server_common::MetricsRecorder;
using privacy_sandbox::server_common::ScopeLatencyRecorder;
using v1::GetValuesRequest;
using v1::GetValuesResponse;
using v1::KeyValueService;

grpc::ServerUnaryReactor* KeyValueServiceImpl::GetValues(
    CallbackServerContext* context, const GetValuesRequest* request,
    GetValuesResponse* response) {
  ScopeLatencyRecorder latency_recorder(std::string(kGetValuesV1Latency),
                                        metrics_recorder_);

  grpc::Status status = handler_.GetValues(*request, response);

  if (status.ok()) {
    metrics_recorder_.IncrementEventStatus(kGetValuesSuccess, absl::OkStatus());
  } else {
    // TODO: use implicit conversion when it becomes available externally
    // https://g3doc.corp.google.com/net/grpc/g3doc/grpc_prod/cpp/status_mapping.md?cl=head
    absl::StatusCode absl_status_code =
        static_cast<absl::StatusCode>(status.error_code());
    absl::Status absl_status =
        absl::Status(absl_status_code, status.error_message());
    metrics_recorder_.IncrementEventStatus(kGetValuesSuccess, absl_status);
  }

  auto* reactor = context->DefaultReactor();
  reactor->Finish(status);
  return reactor;
}

}  // namespace kv_server
