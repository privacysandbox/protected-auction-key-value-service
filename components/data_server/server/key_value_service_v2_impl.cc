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
#include "src/telemetry/telemetry.h"

namespace kv_server {
namespace {

using grpc::CallbackServerContext;
using privacy_sandbox::server_common::GetTracer;
using v2::GetValuesHttpRequest;
using v2::KeyValueService;

template <typename RequestT, typename ResponseT>
using HandlerFunctionT = grpc::Status (GetValuesV2Handler::*)(
    RequestContextFactory&, const RequestT&, ResponseT*,
    ExecutionMetadata& execution_metadata, bool single_partition_use_case,
    GetValuesV2Handler::ContentType content_type) const;

inline void LogTotalExecutionWithoutCustomCodeMetric(
    const privacy_sandbox::server_common::Stopwatch& stopwatch,
    std::optional<int64_t> custom_code_total_execution_time_micros,
    RequestContextFactory& request_context_factory) {
  auto duration_micros = absl::ToDoubleMicroseconds(stopwatch.GetElapsedTime());
  if (custom_code_total_execution_time_micros.has_value()) {
    duration_micros -= *custom_code_total_execution_time_micros;
  }
  UdfRequestMetricsContext& metrics_context =
      request_context_factory.Get().GetUdfRequestMetricsContext();
  LogIfError(metrics_context.LogHistogram<kTotalV2LatencyWithoutCustomCode>(
      (duration_micros)));
}

template <typename RequestT, typename ResponseT>
grpc::ServerUnaryReactor* HandleRequest(
    RequestContextFactory& request_context_factory,
    CallbackServerContext* context, const RequestT* request,
    ResponseT* response, bool is_single_partition_use_case,
    const GetValuesV2Handler& handler,
    HandlerFunctionT<RequestT, ResponseT> handler_function) {
  privacy_sandbox::server_common::Stopwatch stopwatch;
  ExecutionMetadata execution_metadata;
  auto content_type = GetValuesV2Handler::GetContentType(
      context->client_metadata(),
      /*default_content_type=*/GetValuesV2Handler::ContentType::kProto);
  grpc::Status status = (handler.*handler_function)(
      request_context_factory, *request, response, execution_metadata,
      is_single_partition_use_case, content_type);
  auto* reactor = context->DefaultReactor();
  reactor->Finish(status);
  LogRequestCommonSafeMetrics(request, response, status, stopwatch);
  LogTotalExecutionWithoutCustomCodeMetric(
      stopwatch, execution_metadata.custom_code_total_execution_time_micros,
      request_context_factory);
  return reactor;
}

}  // namespace

grpc::ServerUnaryReactor* KeyValueServiceV2Impl::GetValuesHttp(
    CallbackServerContext* context, const GetValuesHttpRequest* request,
    google::api::HttpBody* response) {
  privacy_sandbox::server_common::Stopwatch stopwatch;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  ExecutionMetadata execution_metadata;
  grpc::Status status = handler_.GetValuesHttp(
      *request_context_factory, context->client_metadata(), *request, response,
      execution_metadata);
  auto* reactor = context->DefaultReactor();
  reactor->Finish(status);
  LogRequestCommonSafeMetrics(request, response, status, stopwatch);
  LogTotalExecutionWithoutCustomCodeMetric(
      stopwatch, execution_metadata.custom_code_total_execution_time_micros,
      *request_context_factory);
  return reactor;
}
grpc::ServerUnaryReactor* KeyValueServiceV2Impl::GetValues(
    grpc::CallbackServerContext* context, const v2::GetValuesRequest* request,
    v2::GetValuesResponse* response) {
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  return HandleRequest(*request_context_factory, context, request, response,
                       IsSinglePartitionUseCase(*request), handler_,
                       &GetValuesV2Handler::GetValues);
}

grpc::ServerUnaryReactor* KeyValueServiceV2Impl::ObliviousGetValues(
    CallbackServerContext* context,
    const v2::ObliviousGetValuesRequest* request,
    google::api::HttpBody* response) {
  privacy_sandbox::server_common::Stopwatch stopwatch;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  ExecutionMetadata execution_metadata;
  grpc::Status status = handler_.ObliviousGetValues(
      *request_context_factory, context->client_metadata(), *request, response,
      execution_metadata);
  auto* reactor = context->DefaultReactor();
  reactor->Finish(status);
  LogRequestCommonSafeMetrics(request, response, status, stopwatch);
  LogTotalExecutionWithoutCustomCodeMetric(
      stopwatch, execution_metadata.custom_code_total_execution_time_micros,
      *request_context_factory);
  return reactor;
}

}  // namespace kv_server
