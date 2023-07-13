// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/udf/code_config.h"

namespace kv_server {

bool operator==(const CodeConfig& lhs_config, const CodeConfig& rhs_config) {
  return lhs_config.logical_commit_time == rhs_config.logical_commit_time &&
         lhs_config.udf_handler_name == rhs_config.udf_handler_name &&
         lhs_config.js == rhs_config.js && lhs_config.wasm == rhs_config.wasm;
}

bool operator!=(const CodeConfig& lhs_config, const CodeConfig& rhs_config) {
  return !operator==(lhs_config, rhs_config);
}

}  // namespace kv_server
