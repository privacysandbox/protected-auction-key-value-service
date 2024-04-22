// Copyright 2022 Google LLC
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

#include "components/data_server/server/parameter_fetcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "components/errors/retry.h"
#include "public/constants.h"

namespace kv_server {

ParameterFetcher::ParameterFetcher(
    std::string environment, const ParameterClient& parameter_client,
    absl::AnyInvocable<void(const absl::Status& status, int count) const>
        metrics_callback,
    privacy_sandbox::server_common::log::PSLogContext& log_context)
    : environment_(std::move(environment)),
      parameter_client_(parameter_client),
      metrics_callback_(std::move(metrics_callback)),
      log_context_(log_context) {}

std::string ParameterFetcher::GetParameter(
    std::string_view parameter_suffix,
    std::optional<std::string> default_value) const {
  const std::string param_name = GetParamName(parameter_suffix);
  return TraceRetryUntilOk(
      [this, &param_name, &default_value] {
        return parameter_client_.GetParameter(param_name, default_value);
      },
      "GetParameter", metrics_callback_, log_context_, {{"param", param_name}});
}

int32_t ParameterFetcher::GetInt32Parameter(
    std::string_view parameter_suffix) const {
  const std::string param_name = GetParamName(parameter_suffix);
  return TraceRetryUntilOk(
      [this, &param_name] {
        return parameter_client_.GetInt32Parameter(param_name);
      },
      "GetParameter", metrics_callback_, log_context_, {{"param", param_name}});
}

bool ParameterFetcher::GetBoolParameter(
    std::string_view parameter_suffix) const {
  const std::string param_name = GetParamName(parameter_suffix);
  return TraceRetryUntilOk(
      [this, &param_name] {
        return parameter_client_.GetBoolParameter(param_name);
      },
      "GetParameter", metrics_callback_, log_context_, {{"param", param_name}});
}

std::string ParameterFetcher::GetParamName(
    std::string_view parameter_suffix) const {
  const std::vector<std::string_view> v = {kServiceName, environment_,
                                           parameter_suffix};
  return absl::StrJoin(v, "-");
}

}  // namespace kv_server
