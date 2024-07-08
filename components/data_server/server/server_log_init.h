/*
 * Copyright 2024 Google LLC
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

#ifndef COMPONENTS_DATA_SERVER_SERVER_SERVER_LOG_INIT_H_H_
#define COMPONENTS_DATA_SERVER_SERVER_SERVER_LOG_INIT_H_H_

#include <optional>
#include <string>

#include "components/cloud_config/parameter_client.h"

namespace kv_server {

void InitLog();
absl::optional<std::string> GetMetricsCollectorEndPoint(
    const ParameterClient& parameter_client, const std::string& environment);

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_SERVER_SERVER_LOG_INIT_H_H_
