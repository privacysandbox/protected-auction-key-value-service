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
#include <thread>

#include "aws/ssm/SSMClient.h"
#include "aws/ssm/SSMErrors.h"
#include "aws/ssm/model/GetParameterRequest.h"
#include "components/cloud_config/parameter_client.h"
#include "components/util/platform_initializer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

using testing::_;
using testing::Return;

class MockSsmClient : public ::Aws::SSM::SSMClient {
 public:
  MOCK_METHOD(Aws::SSM::Model::GetParameterOutcome, GetParameter,
              (const Aws::SSM::Model::GetParameterRequest& request),
              (const, override));
};

class ParameterClientAwsTest : public ::testing::Test {
 private:
  PlatformInitializer initializer_;
};

Aws::SSM::Model::GetParameterResult BuildParameterResult(std::string value) {
  Aws::SSM::Model::Parameter parameter;
  parameter.SetValue(std::move(value));
  Aws::SSM::Model::GetParameterResult result;
  result.SetParameter(parameter);
  return result;
}

TEST_F(ParameterClientAwsTest, GetInt32ParameterSuccess) {
  auto ssm_client = std::make_unique<MockSsmClient>();
  auto parameter_result = BuildParameterResult("1");
  Aws::SSM::Model::GetParameterOutcome outcome(parameter_result);
  EXPECT_CALL(*ssm_client, GetParameter(_)).WillOnce(Return(outcome));

  ParameterClient::ClientOptions options;
  options.client_for_unit_testing_ = ssm_client.release();

  auto parameter_client = ParameterClient::Create(options);
  auto result_or_status = parameter_client->GetInt32Parameter("my_param");
  EXPECT_TRUE(result_or_status.ok());
  EXPECT_EQ(result_or_status.value(), 1);
}

TEST_F(ParameterClientAwsTest, GetInt32ParameterSSMClientFailsReturnsError) {
  auto ssm_client = std::make_unique<MockSsmClient>();
  Aws::SSM::SSMError ssm_error;
  Aws::SSM::Model::GetParameterOutcome outcome(ssm_error);
  EXPECT_CALL(*ssm_client, GetParameter(_)).WillOnce(Return(outcome));

  ParameterClient::ClientOptions options;
  options.client_for_unit_testing_ = ssm_client.release();

  auto parameter_client = ParameterClient::Create(options);
  auto result_or_status = parameter_client->GetInt32Parameter("my_param");
  EXPECT_FALSE(result_or_status.ok());
}

TEST_F(ParameterClientAwsTest, GetInt32ParameterNotIntReturnsError) {
  auto ssm_client = std::make_unique<MockSsmClient>();
  auto parameter_result = BuildParameterResult("not an int");
  Aws::SSM::Model::GetParameterOutcome outcome(parameter_result);
  EXPECT_CALL(*ssm_client, GetParameter(_)).WillOnce(Return(outcome));

  ParameterClient::ClientOptions options;
  options.client_for_unit_testing_ = ssm_client.release();

  auto parameter_client = ParameterClient::Create(options);
  auto result_or_status = parameter_client->GetInt32Parameter("my_param");
  EXPECT_FALSE(result_or_status.ok());
}

TEST_F(ParameterClientAwsTest, GetBoolParameterSuccess) {
  auto ssm_client = std::make_unique<MockSsmClient>();
  auto parameter_result = BuildParameterResult("true");
  Aws::SSM::Model::GetParameterOutcome outcome(parameter_result);
  EXPECT_CALL(*ssm_client, GetParameter(_)).WillOnce(Return(outcome));

  ParameterClient::ClientOptions options;
  options.client_for_unit_testing_ = ssm_client.release();

  auto parameter_client = ParameterClient::Create(options);
  auto result_or_status = parameter_client->GetBoolParameter("my_param");
  EXPECT_TRUE(result_or_status.ok());
  EXPECT_TRUE(result_or_status.value());
}

TEST_F(ParameterClientAwsTest, GetBoolParameterSSMClientFailsReturnsError) {
  auto ssm_client = std::make_unique<MockSsmClient>();
  Aws::SSM::SSMError ssm_error;
  Aws::SSM::Model::GetParameterOutcome outcome(ssm_error);
  EXPECT_CALL(*ssm_client, GetParameter(_)).WillOnce(Return(outcome));

  ParameterClient::ClientOptions options;
  options.client_for_unit_testing_ = ssm_client.release();

  auto parameter_client = ParameterClient::Create(options);
  auto result_or_status = parameter_client->GetBoolParameter("my_param");
  EXPECT_FALSE(result_or_status.ok());
}

TEST_F(ParameterClientAwsTest, GetBoolParameterNotBoolReturnsError) {
  auto ssm_client = std::make_unique<MockSsmClient>();
  auto parameter_result = BuildParameterResult("not a bool");
  Aws::SSM::Model::GetParameterOutcome outcome(parameter_result);
  EXPECT_CALL(*ssm_client, GetParameter(_)).WillOnce(Return(outcome));

  ParameterClient::ClientOptions options;
  options.client_for_unit_testing_ = ssm_client.release();

  auto parameter_client = ParameterClient::Create(options);
  auto result_or_status = parameter_client->GetBoolParameter("my_param");
  EXPECT_FALSE(result_or_status.ok());
}

}  // namespace

}  // namespace kv_server
