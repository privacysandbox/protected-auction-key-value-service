/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TOOLS_REQUEST_SIMULATION_REQUEST_GENERATION_UTIL_H_
#define TOOLS_REQUEST_SIMULATION_REQUEST_GENERATION_UTIL_H_

#include <string>
#include <vector>

#include "tools/request_simulation/request/raw_request.pb.h"

namespace kv_server {

// Generates random keys based on the number of keys and size of each key
std::vector<std::string> GenerateRandomKeys(int number_of_keys, int key_size);

// Creates KV DSP request body in json
std::string CreateKVDSPRequestBodyInJson(
    const std::vector<std::string>& keys,
    std::string_view consented_debug_token,
    std::optional<std::string> generation_id_override = std::nullopt);

// Creates proto message from request body in json
kv_server::RawRequest CreatePlainTextRequest(
    const std::string& request_in_json);

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_REQUEST_GENERATION_UTIL_H_
