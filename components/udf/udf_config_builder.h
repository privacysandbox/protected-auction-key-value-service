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
#include <memory>

#include "components/udf/hooks/get_values_hook.h"
#include "components/udf/hooks/run_query_hook.h"
#include "src/roma/config/config.h"

namespace kv_server {

class UdfConfigBuilder {
 public:
  UdfConfigBuilder& RegisterStringGetValuesHook(GetValuesHook& get_values_hook);

  UdfConfigBuilder& RegisterBinaryGetValuesHook(GetValuesHook& get_values_hook);

  UdfConfigBuilder& RegisterRunSetQueryStringHook(
      RunSetQueryStringHook& run_query_hook);

  UdfConfigBuilder& RegisterRunSetQueryUInt32Hook(
      RunSetQueryUInt32Hook& run_set_query_uint32_hook);

  UdfConfigBuilder& RegisterRunSetQueryUInt64Hook(
      RunSetQueryUInt64Hook& run_set_query_uint64_hook);

  UdfConfigBuilder& RegisterLoggingFunction();

  UdfConfigBuilder& SetNumberOfWorkers(int number_of_workers);

  google::scp::roma::Config<std::weak_ptr<RequestContext>>& Config();

 private:
  google::scp::roma::Config<std::weak_ptr<RequestContext>> config_;
};
}  // namespace kv_server
