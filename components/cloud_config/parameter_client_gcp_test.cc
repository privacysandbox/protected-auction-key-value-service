/*
 * Copyright 2023 Google LLC
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

#include <future>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "components/cloud_config/parameter_client.h"
#include "gtest/gtest.h"
#include "src/public/cpio/interface/error_codes.h"
#include "src/public/cpio/interface/parameter_client/parameter_client_interface.h"
#include "src/public/cpio/mock/parameter_client/mock_parameter_client.h"

namespace kv_server {
namespace {

using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::Callback;
using google::scp::cpio::MockParameterClient;
using google::scp::cpio::ParameterClientInterface;
using google::scp::cpio::ParameterClientOptions;
using testing::Return;

class ParameterClientGcpTest : public ::testing::Test {
 protected:
  absl::flat_hash_map<std::string, std::string> expected_param_values_ = {
      {"string_test_flag", "string_test_val"},
      {"bool_test_flag", "true"},
      {"int32_test_flag", "2023"},
      {"empty_string_flag", "EMPTY_STRING"},
      {"invalid_bool_flag", "trlse"},
      {"invalid_int32_flag", "twenty_three"}};
  std::vector<std::future<void>> f_;
  std::unique_ptr<ParameterClient> gcp_parameter_client_;

  ParameterClientGcpTest() {
    std::unique_ptr<MockParameterClient> mock_parameter_client =
        std::make_unique<MockParameterClient>();
    EXPECT_CALL(*mock_parameter_client, Init)
        .WillOnce(Return(absl::OkStatus()));
    EXPECT_CALL(*mock_parameter_client, Run).WillOnce(Return(absl::OkStatus()));
    EXPECT_CALL(*mock_parameter_client, GetParameter)
        .WillRepeatedly(
            [this](GetParameterRequest get_param_req,
                   Callback<GetParameterResponse> callback) -> absl::Status {
              // async reading parameter like the real case
              bool param_not_found = false;
              if (expected_param_values_.find(get_param_req.parameter_name()) ==
                  expected_param_values_.end()) {
                param_not_found = true;
              }
              f_.push_back(std::async(
                  std::launch::async,
                  [cb = std::move(callback), req = std::move(get_param_req),
                   &param_not_found, this]() {
                    absl::SleepFor(absl::Milliseconds(100));  // simulate delay
                    GetParameterResponse response;
                    if (param_not_found) {
                      cb(FailureExecutionResult(5), response);
                    } else {
                      response.set_parameter_value(
                          expected_param_values_.at(req.parameter_name()));
                      cb(SuccessExecutionResult(), response);
                    }
                  }));
              return param_not_found
                         ? absl::NotFoundError("Parameter not found.")
                         : absl::OkStatus();
            });

    ParameterClient::ClientOptions client_options;
    client_options.client_for_unit_testing_ = mock_parameter_client.release();
    EXPECT_TRUE(client_options.client_for_unit_testing_ != nullptr);

    gcp_parameter_client_ = ParameterClient::Create(std::move(client_options));
    for (auto& each : f_) {
      each.get();
    }
  }
};

TEST_F(ParameterClientGcpTest, GetParametersSuccess) {
  auto string_param = gcp_parameter_client_->GetParameter("string_test_flag");
  EXPECT_TRUE(string_param.ok());
  EXPECT_EQ(string_param.value(), "string_test_val");
}

TEST_F(ParameterClientGcpTest, GetBoolParametersSuccess) {
  auto bool_param = gcp_parameter_client_->GetBoolParameter("bool_test_flag");
  EXPECT_TRUE(bool_param.ok());
  EXPECT_TRUE(bool_param.value());
}

TEST_F(ParameterClientGcpTest, GetInt32ParametersSuccess) {
  auto int32_param =
      gcp_parameter_client_->GetInt32Parameter("int32_test_flag");
  EXPECT_TRUE(int32_param.ok());
  EXPECT_EQ(int32_param.value(), 2023);
}

TEST_F(ParameterClientGcpTest, GetParametersNotFound) {
  auto string_param = gcp_parameter_client_->GetParameter("invalid_test_flag");
  EXPECT_FALSE(string_param.ok());
}

TEST_F(ParameterClientGcpTest, GetParametersEmptyStringSuccess) {
  auto string_param = gcp_parameter_client_->GetParameter("empty_string_flag");
  EXPECT_TRUE(string_param.ok());
  EXPECT_TRUE(string_param.value().empty());
}

TEST_F(ParameterClientGcpTest, GetBoolParameterInvalidReturn) {
  auto bool_param =
      gcp_parameter_client_->GetBoolParameter("invalid_bool_flag");
  EXPECT_FALSE(bool_param.ok());
}

TEST_F(ParameterClientGcpTest, GetInt32ParameterInvalidReturn) {
  auto int32_param =
      gcp_parameter_client_->GetInt32Parameter("invalid_int32_flag");
  EXPECT_FALSE(int32_param.ok());
}

}  // namespace
}  // namespace kv_server
