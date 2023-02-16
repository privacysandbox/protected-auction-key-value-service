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

namespace kv_server {

namespace {
constexpr std::string_view kParameterPrefix = "kv-server";

class ParameterFetcherImpl : public ParameterFetcher {
 public:
  ParameterFetcherImpl(std::string environment,
                       const ParameterClient& parameter_client,
                       MetricsRecorder& metrics_recorder)
      : environment_(std::move(environment)),
        parameter_client_(parameter_client),
        metrics_recorder_(metrics_recorder) {}

  std::string GetParameter(std::string_view parameter_suffix) const override {
    const std::string param_name = GetParamName(parameter_suffix);
    return TraceRetryUntilOk(
        [this, &param_name] {
          return parameter_client_.GetParameter(param_name);
        },
        "GetParameter", metrics_recorder_, {{"param", param_name}});
  }

  int32_t GetInt32Parameter(std::string_view parameter_suffix) const override {
    const std::string param_name = GetParamName(parameter_suffix);
    return TraceRetryUntilOk(
        [this, &param_name] {
          return parameter_client_.GetInt32Parameter(param_name);
        },
        "GetParameter", metrics_recorder_, {{"param", param_name}});
  }

 private:
  const std::string environment_;
  const ParameterClient& parameter_client_;
  MetricsRecorder& metrics_recorder_;

  std::string GetParamName(std::string_view parameter_suffix) const {
    const std::vector<std::string_view> v = {kParameterPrefix, environment_,
                                             parameter_suffix};
    return absl::StrJoin(v, "-");
  }
};

}  // namespace

std::unique_ptr<ParameterFetcher> ParameterFetcher::Create(
    std::string environment, const ParameterClient& parameter_client,
    MetricsRecorder& metrics_recorder) {
  return std::make_unique<ParameterFetcherImpl>(
      std::move(environment), parameter_client, metrics_recorder);
}

}  // namespace kv_server
