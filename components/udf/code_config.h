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

#ifndef COMPONENTS_UDF_CODE_CONFIG_H_
#define COMPONENTS_UDF_CODE_CONFIG_H_

#include <string>

namespace kv_server {

// Adtech configurable properties of a UDF code object.
struct CodeConfig {
  // Only one of js or wasm needs to be set.
  // If both are, js will have priority.
  std::string js;
  std::string wasm;
  std::string udf_handler_name;
};

}  // namespace kv_server

#endif  // COMPONENTS_UDF_CODE_CONFIG_H_
