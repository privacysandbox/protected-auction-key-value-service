// Copyright 2023 Google LLC
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

#include "components/udf/udf_config_builder.h"

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "components/udf/hooks/get_values_hook.h"
#include "components/udf/hooks/logging_hook.h"
#include "components/udf/hooks/run_query_hook.h"
#include "src/roma/config/config.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/roma.h"

namespace kv_server {
namespace {

using google::scp::roma::Config;
using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::FunctionBindingPayload;

constexpr char kStringGetValuesHookJsName[] = "getValues";
constexpr char kBinaryGetValuesHookJsName[] = "getValuesBinary";
constexpr char kRunQueryHookJsName[] = "runQuery";
constexpr char kRunSetQueryUInt32HookJsName[] = "runSetQueryUInt32";
constexpr char kRunSetQueryUInt64HookJsName[] = "runSetQueryUInt64";
constexpr char kLoggingHookJsName[] = "logMessage";

std::unique_ptr<FunctionBindingObjectV2<std::weak_ptr<RequestContext>>>
GetValuesFunctionObject(GetValuesHook& get_values_hook,
                        std::string handler_name) {
  auto get_values_function_object = std::make_unique<
      FunctionBindingObjectV2<std::weak_ptr<RequestContext>>>();
  get_values_function_object->function_name = std::move(handler_name);
  get_values_function_object->function =
      [&get_values_hook](
          FunctionBindingPayload<std::weak_ptr<RequestContext>>& in) {
        get_values_hook(in);
      };
  return get_values_function_object;
}

}  // namespace

UdfConfigBuilder& UdfConfigBuilder::RegisterStringGetValuesHook(
    GetValuesHook& get_values_hook) {
  config_.RegisterFunctionBinding(
      GetValuesFunctionObject(get_values_hook, kStringGetValuesHookJsName));
  return *this;
}

UdfConfigBuilder& UdfConfigBuilder::RegisterBinaryGetValuesHook(
    GetValuesHook& get_values_hook) {
  config_.RegisterFunctionBinding(
      GetValuesFunctionObject(get_values_hook, kBinaryGetValuesHookJsName));
  return *this;
}

UdfConfigBuilder& UdfConfigBuilder::RegisterRunSetQueryStringHook(
    RunSetQueryStringHook& run_query_hook) {
  auto run_query_function_object = std::make_unique<
      FunctionBindingObjectV2<std::weak_ptr<RequestContext>>>();
  run_query_function_object->function_name = kRunQueryHookJsName;
  run_query_function_object->function =
      [&run_query_hook](
          FunctionBindingPayload<std::weak_ptr<RequestContext>>& in) {
        run_query_hook(in);
      };
  config_.RegisterFunctionBinding(std::move(run_query_function_object));
  return *this;
}

UdfConfigBuilder& UdfConfigBuilder::RegisterRunSetQueryUInt32Hook(
    RunSetQueryUInt32Hook& run_set_query_uint32_hook) {
  auto run_query_function_object = std::make_unique<
      FunctionBindingObjectV2<std::weak_ptr<RequestContext>>>();
  run_query_function_object->function_name = kRunSetQueryUInt32HookJsName;
  run_query_function_object->function =
      [&run_set_query_uint32_hook](
          FunctionBindingPayload<std::weak_ptr<RequestContext>>& in) {
        run_set_query_uint32_hook(in);
      };
  config_.RegisterFunctionBinding(std::move(run_query_function_object));
  return *this;
}

UdfConfigBuilder& UdfConfigBuilder::RegisterRunSetQueryUInt64Hook(
    RunSetQueryUInt64Hook& run_set_query_uint64_hook) {
  auto run_query_function_object = std::make_unique<
      FunctionBindingObjectV2<std::weak_ptr<RequestContext>>>();
  run_query_function_object->function_name = kRunSetQueryUInt64HookJsName;
  run_query_function_object->function =
      [&run_set_query_uint64_hook](
          FunctionBindingPayload<std::weak_ptr<RequestContext>>& in) {
        run_set_query_uint64_hook(in);
      };
  config_.RegisterFunctionBinding(std::move(run_query_function_object));
  return *this;
}

UdfConfigBuilder& UdfConfigBuilder::RegisterLoggingHook() {
  auto logging_function_object = std::make_unique<
      FunctionBindingObjectV2<std::weak_ptr<RequestContext>>>();
  logging_function_object->function_name = kLoggingHookJsName;
  logging_function_object->function = LogMessage;
  config_.RegisterFunctionBinding(std::move(logging_function_object));
  return *this;
}

UdfConfigBuilder& UdfConfigBuilder::SetNumberOfWorkers(
    const int number_of_workers) {
  config_.number_of_workers = number_of_workers;
  return *this;
}

google::scp::roma::Config<std::weak_ptr<RequestContext>>&
UdfConfigBuilder::Config() {
  return config_;
}

}  // namespace kv_server
