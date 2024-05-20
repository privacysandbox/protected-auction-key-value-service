/*
 * Copyright 2023 Google LLC
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

#ifndef COMPONENTS_UDF_RUN_QUERY_HOOK_H_
#define COMPONENTS_UDF_RUN_QUERY_HOOK_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "components/internal_server/lookup.h"
#include "components/util/request_context.h"
#include "nlohmann/json.hpp"
#include "src/roma/config/function_binding_object_v2.h"

namespace kv_server {

// Functor that acts as a wrapper for the internal query client call.
template <typename ResponseType>
class RunSetQueryHook {
 public:
  // We need to split the hook init, since lookup depends on the cache.
  // However, UdfClient init requires the hook and it also forks.
  // The cache is only initialized after UdfClient init, so the hook
  // init can only be completed after UdfClient and cache init.
  void FinishInit(std::unique_ptr<Lookup> lookup);
  // This is registered with v8 and is exposed to the UDF. Internally, it calls
  // the internal query client.
  void operator()(
      google::scp::roma::FunctionBindingPayload<std::weak_ptr<RequestContext>>&
          payload);
  static std::unique_ptr<RunSetQueryHook<ResponseType>> Create();

 private:
  constexpr std::string_view HookName();
  void ReportErrorStatus(
      absl::StatusCode error_code, std::string_view error_message,
      google::scp::roma::FunctionBindingPayload<std::weak_ptr<RequestContext>>&
          payload);

  // `lookup_` is initialized separately, since its dependencies create
  // threads. Lazy load is used to ensure that it only happens after Roma
  // forks.
  std::unique_ptr<Lookup> lookup_;
};

template <typename ResponseType>
constexpr std::string_view RunSetQueryHook<ResponseType>::HookName() {
  if constexpr (std::is_same_v<ResponseType, InternalRunQueryResponse>) {
    return "runQuery";
  }
  if constexpr (std::is_same_v<ResponseType, InternalRunSetQueryIntResponse>) {
    return "runSetQueryInt";
  }
}

template <typename ResponseType>
void RunSetQueryHook<ResponseType>::ReportErrorStatus(
    absl::StatusCode error_code, std::string_view error_message,
    google::scp::roma::FunctionBindingPayload<std::weak_ptr<RequestContext>>&
        payload) {
  nlohmann::json status;
  status["code"] = error_code;
  status["message"] = error_message;
  payload.io_proto.mutable_output_list_of_string()->add_data(status.dump());
}

template <typename ResponseType>
void RunSetQueryHook<ResponseType>::FinishInit(std::unique_ptr<Lookup> lookup) {
  if (lookup_ == nullptr) {
    lookup_ = std::move(lookup);
  }
}

template <typename ResponseType>
void RunSetQueryHook<ResponseType>::operator()(
    google::scp::roma::FunctionBindingPayload<std::weak_ptr<RequestContext>>&
        payload) {
  std::shared_ptr<RequestContext> request_context = payload.metadata.lock();
  if (request_context == nullptr) {
    PS_VLOG(1) << "Request context is not available, the request might "
                  "have been marked as complete";
    return;
  }
  if (lookup_ == nullptr) {
    ReportErrorStatus(absl::StatusCode::kInternal,
                      absl::StrCat(HookName(), " has not been initialized yet"),
                      payload);
    PS_LOG(ERROR, request_context->GetPSLogContext()) << absl::StrCat(
        HookName(), " hook is not initialized properly: lookup is nullptr");
    return;
  }
  PS_VLOG(9, request_context->GetPSLogContext())
      << HookName() << " request: " << payload.io_proto.DebugString();
  if (!payload.io_proto.has_input_string()) {
    ReportErrorStatus(absl::StatusCode::kInvalidArgument,
                      absl::StrCat(HookName(), " input must be a string"),
                      payload);
    PS_VLOG(1, request_context->GetPSLogContext())
        << HookName() << " result: " << payload.io_proto.DebugString();
    return;
  }
  PS_VLOG(9, request_context->GetPSLogContext())
      << "Calling internal " << HookName() << " client";
  absl::StatusOr<ResponseType> response_or_status;
  if constexpr (std::is_same_v<ResponseType, InternalRunQueryResponse>) {
    response_or_status =
        lookup_->RunQuery(*request_context, payload.io_proto.input_string());
  }
  if constexpr (std::is_same_v<ResponseType, InternalRunSetQueryIntResponse>) {
    response_or_status = lookup_->RunSetQueryInt(
        *request_context, payload.io_proto.input_string());
  }
  if (!response_or_status.ok()) {
    PS_LOG(ERROR, request_context->GetPSLogContext())
        << "Internal " << HookName()
        << " returned error: " << response_or_status.status();
    const auto& status = response_or_status.status();
    ReportErrorStatus(
        status.code(),
        absl::StrCat(HookName(), " failed with error: ", status.message()),
        payload);
    PS_VLOG(1, request_context->GetPSLogContext())
        << HookName() << " result: " << payload.io_proto.DebugString();
    return;
  }
  PS_VLOG(9, request_context->GetPSLogContext())
      << "Processing internal " << HookName() << " response";
  if constexpr (std::is_same_v<ResponseType, InternalRunQueryResponse>) {
    *payload.io_proto.mutable_output_list_of_string()->mutable_data() =
        std::move(*response_or_status.value().mutable_elements());
  }
  if constexpr (std::is_same_v<ResponseType, InternalRunSetQueryIntResponse>) {
    const auto& elements = response_or_status->elements();
    payload.io_proto.set_output_bytes(elements.data(),
                                      elements.size() * sizeof(uint32_t));
  }
  PS_VLOG(9, request_context->GetPSLogContext())
      << HookName() << " result: " << payload.io_proto.DebugString();
}

template <typename ResponseType>
std::unique_ptr<RunSetQueryHook<ResponseType>>
RunSetQueryHook<ResponseType>::Create() {
  return std::make_unique<RunSetQueryHook<ResponseType>>();
}

using RunSetQueryStringHook = RunSetQueryHook<InternalRunQueryResponse>;
using RunSetQueryIntHook = RunSetQueryHook<InternalRunSetQueryIntResponse>;

}  // namespace kv_server

#endif  // COMPONENTS_UDF_RUN_QUERY_HOOK_H_
