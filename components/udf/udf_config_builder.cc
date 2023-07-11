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

#include "components/udf/get_values_hook.h"
#include "components/udf/logging_hook.h"
#include "components/udf/run_query_hook.h"
#include "roma/config/src/config.h"
#include "roma/config/src/function_binding_object.h"
#include "roma/interface/roma.h"

namespace kv_server {
using google::scp::roma::Config;
using google::scp::roma::FunctionBindingObject;

constexpr char kGetValuesHookJsName[] = "getValues";
constexpr char kRunQueryHookJsName[] = "runQuery";
constexpr char kLoggingHookJsName[] = "logMessage";

UdfConfigBuilder& UdfConfigBuilder::RegisterGetValuesHook(
    GetValuesHook& get_values_hook) {
  auto get_values_function_object = std::make_unique<
      FunctionBindingObject<std::string, std::vector<std::string>>>();
  get_values_function_object->function_name = kGetValuesHookJsName;
  get_values_function_object->function =
      [&get_values_hook](
          std::tuple<std::vector<std::string>>& in) -> std::string {
    return get_values_hook(in);
  };
  config_.RegisterFunctionBinding(std::move(get_values_function_object));
  return *this;
}

UdfConfigBuilder& UdfConfigBuilder::RegisterRunQueryHook(
    RunQueryHook& run_query_hook) {
  auto run_query_function_object = std::make_unique<
      FunctionBindingObject<std::vector<std::string>, std::string>>();
  run_query_function_object->function_name = kRunQueryHookJsName;
  run_query_function_object->function =
      [&run_query_hook](
          std::tuple<std::string>& in) -> std::vector<std::string> {
    return run_query_hook(in);
  };
  config_.RegisterFunctionBinding(std::move(run_query_function_object));
  return *this;
}

UdfConfigBuilder& UdfConfigBuilder::RegisterLoggingHook() {
  auto logging_function_object =
      std::make_unique<FunctionBindingObject<std::string, std::string>>();
  logging_function_object->function_name = kLoggingHookJsName;
  logging_function_object->function = LogMessage;
  config_.RegisterFunctionBinding(std::move(logging_function_object));
  return *this;
}

UdfConfigBuilder& UdfConfigBuilder::SetNumberOfWorkers(
    const int number_of_workers) {
  config_.NumberOfWorkers = number_of_workers;
  return *this;
}

const google::scp::roma::Config& UdfConfigBuilder::Config() const {
  return config_;
}

}  // namespace kv_server
