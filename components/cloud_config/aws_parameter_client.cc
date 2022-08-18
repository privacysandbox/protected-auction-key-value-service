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
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "aws/core/Aws.h"
#include "aws/core/utils/Outcome.h"
#include "aws/ssm/SSMClient.h"
#include "aws/ssm/model/GetParameterRequest.h"
#include "aws/ssm/model/GetParameterResult.h"
#include "components/cloud_config/parameter_client.h"
#include "components/errors/aws_error_util.h"
#include "glog/logging.h"

namespace fledge::kv_server {
namespace {

class AwsParameterClient : public ParameterClient {
 public:
  absl::StatusOr<std::string> GetParameter(
      std::string_view parameter_name) const override {
    Aws::SSM::Model::GetParameterRequest request;
    request.SetName(std::string(parameter_name));
    const auto outcome = ssm_client_->GetParameter(request);
    if (!outcome.IsSuccess()) {
      return AwsErrorToStatus(outcome.GetError());
    }
    return outcome.GetResult().GetParameter().GetValue();
  };

  AwsParameterClient() : ssm_client_(std::make_unique<Aws::SSM::SSMClient>()) {}

 private:
  std::unique_ptr<Aws::SSM::SSMClient> ssm_client_;
};

}  // namespace

std::unique_ptr<ParameterClient> ParameterClient::Create() {
  return std::make_unique<AwsParameterClient>();
}

}  // namespace fledge::kv_server
