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
#include "roma/config/src/config.h"
#include "roma/config/src/function_binding_object_v2.h"
#include "roma/interface/roma.h"

namespace kv_server {
namespace {

using google::scp::roma::Config;
using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::FunctionBindingPayload;

constexpr char kStringGetValuesHookJsName[] = "getValues";
constexpr char kBinaryGetValuesHookJsName[] = "getValuesBinary";
constexpr char kRunQueryHookJsName[] = "runQuery";

std::unique_ptr<FunctionBindingObjectV2<RequestContext>>
GetValuesFunctionObject(GetValuesHook& get_values_hook,
                        std::string handler_name) {
  auto get_values_function_object =
      std::make_unique<FunctionBindingObjectV2<RequestContext>>();
  get_values_function_object->function_name = std::move(handler_name);
  get_values_function_object->function =
      [&get_values_hook](FunctionBindingPayload<RequestContext>& in) {
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

UdfConfigBuilder& UdfConfigBuilder::RegisterRunQueryHook(
    RunQueryHook& run_query_hook) {
  auto run_query_function_object =
      std::make_unique<FunctionBindingObjectV2<RequestContext>>();
  run_query_function_object->function_name = kRunQueryHookJsName;
  run_query_function_object->function =
      [&run_query_hook](FunctionBindingPayload<RequestContext>& in) {
        run_query_hook(in);
      };
  config_.RegisterFunctionBinding(std::move(run_query_function_object));
  return *this;
}

UdfConfigBuilder& UdfConfigBuilder::RegisterLoggingFunction() {
  config_.SetLoggingFunction(LoggingFunction);
  return *this;
}

UdfConfigBuilder& UdfConfigBuilder::SetNumberOfWorkers(
    const int number_of_workers) {
  config_.number_of_workers = number_of_workers;
  return *this;
}

google::scp::roma::Config<RequestContext>& UdfConfigBuilder::Config() {
  return config_;
}

}  // namespace kv_server
