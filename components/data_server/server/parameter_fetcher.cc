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
  ParameterFetcherImpl(const ParameterClient& parameter_client,
                       std::string environment)
      : parameter_client_(parameter_client),
        environment_(std::move(environment)) {}

  std::string GetParameter(std::string_view parameter_suffix) const override {
    const std::vector<std::string_view> v = {kParameterPrefix, environment_,
                                             parameter_suffix};
    const std::string param_name = absl::StrJoin(v, "-");
    return TraceRetryUntilOk(
        [this, &param_name] {
          return parameter_client_.GetParameter(param_name);
        },
        "GetParameter", {{"param", param_name}});
  }

 private:
  const ParameterClient& parameter_client_;
  const std::string environment_;
};

}  // namespace

std::unique_ptr<ParameterFetcher> ParameterFetcher::Create(
    const ParameterClient& parameter_client, std::string environment) {
  return std::make_unique<ParameterFetcherImpl>(parameter_client,
                                                std::move(environment));
}

}  // namespace kv_server
