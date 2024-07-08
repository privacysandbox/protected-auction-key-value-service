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

// TODO(b/299623229): Swich to CPIO implementation once it supports fetching
// cloud params from local instances

#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "aws/core/Aws.h"
#include "aws/core/utils/Outcome.h"
#include "aws/ssm/SSMClient.h"
#include "aws/ssm/model/GetParameterRequest.h"
#include "aws/ssm/model/GetParameterResult.h"
#include "components/cloud_config/parameter_client.h"
#include "components/errors/error_util_aws.h"

namespace kv_server {
namespace {

class AwsParameterClient : public ParameterClient {
 public:
  absl::StatusOr<std::string> GetParameter(
      std::string_view parameter_name,
      std::optional<std::string> default_value = std::nullopt) const override {
    PS_LOG(INFO, log_context_) << "Getting parameter: " << parameter_name;
    Aws::SSM::Model::GetParameterRequest request;
    request.SetName(std::string(parameter_name));
    const auto outcome = ssm_client_->GetParameter(request);
    if (!outcome.IsSuccess()) {
      if (default_value.has_value()) {
        PS_LOG(WARNING, log_context_)
            << "Unable to get parameter: " << parameter_name
            << " with error: " << outcome.GetError()
            << ", returning default value: " << *default_value;
        return *default_value;
      } else {
        PS_LOG(ERROR, log_context_)
            << "Unable to get parameter: " << parameter_name
            << " with error: " << outcome.GetError();
      }
      return AwsErrorToStatus(outcome.GetError());
    }
    std::string result = outcome.GetResult().GetParameter().GetValue();
    PS_LOG(INFO, log_context_)
        << "Got parameter: " << parameter_name << " with value: " << result;
    return result;
  };

  absl::StatusOr<int32_t> GetInt32Parameter(
      std::string_view parameter_name) const override {
    // https://docs.aws.amazon.com/systems-manager/latest/APIReference/API_GetParameter.html
    // AWS SDK only returns "string" value, so we need to do the conversion
    // ourselves.
    absl::StatusOr<std::string> parameter = GetParameter(parameter_name);

    if (!parameter.ok()) {
      return parameter.status();
    }

    int32_t parameter_int32;
    if (!absl::SimpleAtoi(*parameter, &parameter_int32)) {
      const std::string error =
          absl::StrFormat("Failed converting %s parameter: %s to int32.",
                          parameter_name, *parameter);
      PS_LOG(ERROR, log_context_) << error;
      return absl::InvalidArgumentError(error);
    }

    return parameter_int32;
  };

  absl::StatusOr<bool> GetBoolParameter(
      std::string_view parameter_name) const override {
    // https://docs.aws.amazon.com/systems-manager/latest/APIReference/API_GetParameter.html
    // AWS SDK only returns "string" value, so we need to do the conversion
    // ourselves.
    absl::StatusOr<std::string> parameter = GetParameter(parameter_name);

    if (!parameter.ok()) {
      return parameter.status();
    }

    bool parameter_bool;
    if (!absl::SimpleAtob(*parameter, &parameter_bool)) {
      const std::string error =
          absl::StrFormat("Failed converting %s parameter: %s to bool.",
                          parameter_name, *parameter);
      PS_LOG(ERROR, log_context_) << error;
      return absl::InvalidArgumentError(error);
    }

    return parameter_bool;
  };

  void UpdateLogContext(
      privacy_sandbox::server_common::log::PSLogContext& log_context) override {
    log_context_ = log_context;
  }

  explicit AwsParameterClient(
      ParameterClient::ClientOptions client_options,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : client_options_(std::move(client_options)), log_context_(log_context) {
    if (client_options.client_for_unit_testing_ != nullptr) {
      ssm_client_.reset(
          (Aws::SSM::SSMClient*)client_options.client_for_unit_testing_);
    } else {
      ssm_client_ = std::make_unique<Aws::SSM::SSMClient>();
    }
  }

 private:
  ClientOptions client_options_;
  std::unique_ptr<Aws::SSM::SSMClient> ssm_client_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

}  // namespace

std::unique_ptr<ParameterClient> ParameterClient::Create(
    ParameterClient::ClientOptions client_options,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<AwsParameterClient>(std::move(client_options),
                                              log_context);
}

}  // namespace kv_server
