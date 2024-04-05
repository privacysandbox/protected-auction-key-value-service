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

#ifndef TOOLS_REQUEST_SIMULATION_REQUEST_SIMULATION_PARAMETER_FETCHER_H_
#define TOOLS_REQUEST_SIMULATION_REQUEST_SIMULATION_PARAMETER_FETCHER_H_

#include "components/data/common/msg_svc.h"

namespace kv_server {

class RequestSimulationParameterFetcher {
 public:
  RequestSimulationParameterFetcher() = default;
  virtual ~RequestSimulationParameterFetcher() = default;
  virtual NotifierMetadata GetBlobStorageNotifierMetadata() const;
  virtual NotifierMetadata GetRealtimeNotifierMetadata() const;
};

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_REQUEST_SIMULATION_PARAMETER_FETCHER_H_
