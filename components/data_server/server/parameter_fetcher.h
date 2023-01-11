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

#ifndef COMPONENTS_DATA_SERVER_SERVER_PARAMETER_FETCHER_H_
#define COMPONENTS_DATA_SERVER_SERVER_PARAMETER_FETCHER_H_

#include <memory>
#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "components/cloud_config/parameter_client.h"

namespace kv_server {

class ParameterFetcher {
 public:
  virtual ~ParameterFetcher() = default;

  // This function will retry any necessary requests until it succeeds.
  virtual std::string GetParameter(std::string_view parameter_suffix) const = 0;

  static std::unique_ptr<ParameterFetcher> Create(
      const ParameterClient& parameter_client, std::string environment);
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_SERVER_PARAMETER_FETCHER_H_
