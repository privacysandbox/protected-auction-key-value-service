// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/data_server/request_handler/partitions/single_partition_processor.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "components/udf/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "public/constants.h"
#include "public/test_util/proto_matcher.h"
#include "public/test_util/request_example.h"

#include "cbor.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using grpc::StatusCode;
using testing::_;
using testing::Return;
using testing::ReturnRef;
using testing::UnorderedElementsAre;
using v2::GetValuesHttpRequest;
using v2::ObliviousGetValuesRequest;

struct TestingParameters {
  const std::string_view request_json;
};

class SinglePartitionProcessorTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<TestingParameters> {
 protected:
  void SetUp() override {
    privacy_sandbox::server_common::log::ServerToken(
        kExampleConsentedDebugToken);
    InitMetricsContextMap();
    request_context_factory_ = std::make_unique<RequestContextFactory>();
  }

  v2::GetValuesRequest GetTestRequestBody() {
    v2::GetValuesRequest request_proto;
    EXPECT_TRUE(google::protobuf::util::JsonStringToMessage(
                    GetParam().request_json, &request_proto)
                    .ok());
    return request_proto;
  }

  MockUdfClient mock_udf_client_;
  std::unique_ptr<RequestContextFactory> request_context_factory_;
};

INSTANTIATE_TEST_SUITE_P(
    SinglePartitionProcessorTest, SinglePartitionProcessorTest,
    testing::Values(
        TestingParameters{
            .request_json = kv_server::kExampleV2RequestInJson,
        },
        TestingParameters{
            .request_json = kv_server::kExampleConsentedV2RequestInJson,
        }));

TEST_P(SinglePartitionProcessorTest, Success) {
  UDFExecutionMetadata udf_metadata;
  TextFormat::ParseFromString(kExampleV2RequestUdfMetadata, &udf_metadata);
  UDFArgument udf_arg1, udf_arg2;
  TextFormat::ParseFromString(kExampleV2RequestUdfArg1, &udf_arg1);
  TextFormat::ParseFromString(kExampleV2RequestUdfArg2, &udf_arg2);

  nlohmann::json output =
      nlohmann::json::parse(R"json("any_desired_json_output")json");
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(_, EqualsProto(udf_metadata),
                          testing::ElementsAre(EqualsProto(udf_arg1),
                                               EqualsProto(udf_arg2)),
                          _))
      .WillOnce(Return(output.dump()));

  const auto request = GetTestRequestBody();
  v2::GetValuesResponse response;
  ExecutionMetadata unused_execution_metadata;
  SinglePartitionProcessor processor(*request_context_factory_,
                                     mock_udf_client_);
  const auto status =
      processor.Process(request, response, unused_execution_metadata);
  ASSERT_TRUE(status.ok()) << status;

  v2::GetValuesResponse expected_response;
  expected_response.mutable_single_partition()->set_string_output(
      output.dump());

  EXPECT_THAT(response, EqualsProto(expected_response));
}

TEST_P(SinglePartitionProcessorTest,
       UdfFailureReturnsErrorAndPopulatesResponse) {
  UDFExecutionMetadata udf_metadata;
  TextFormat::ParseFromString(kExampleV2RequestUdfMetadata, &udf_metadata);
  UDFArgument udf_arg1, udf_arg2;
  TextFormat::ParseFromString(kExampleV2RequestUdfArg1, &udf_arg1);
  TextFormat::ParseFromString(kExampleV2RequestUdfArg2, &udf_arg2);

  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(_, EqualsProto(udf_metadata),
                          testing::ElementsAre(EqualsProto(udf_arg1),
                                               EqualsProto(udf_arg2)),
                          _))
      .WillOnce(Return(absl::InternalError("UDF execution error")));

  const auto request = GetTestRequestBody();
  v2::GetValuesResponse response;
  ExecutionMetadata unused_execution_metadata;
  SinglePartitionProcessor processor(*request_context_factory_,
                                     mock_udf_client_);
  const auto status =
      processor.Process(request, response, unused_execution_metadata);
  ASSERT_FALSE(status.ok()) << status;

  v2::GetValuesResponse expected_response;
  auto* resp_status =
      expected_response.mutable_single_partition()->mutable_status();
  resp_status->set_code(13);
  resp_status->set_message("UDF execution error");

  EXPECT_THAT(response, EqualsProto(expected_response));
}

TEST_F(SinglePartitionProcessorTest,
       MoreThanOnePartitionInRequestReturnsError) {
  v2::GetValuesRequest request;
  TextFormat::ParseFromString(R"pb(
                                partitions { id: 0 }
                                partitions { id: 1 }
                              )pb",
                              &request);

  v2::GetValuesResponse response;
  ExecutionMetadata unused_execution_metadata;
  SinglePartitionProcessor processor(*request_context_factory_,
                                     mock_udf_client_);
  const auto status =
      processor.Process(request, response, unused_execution_metadata);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(SinglePartitionProcessorTest, NoPartitionReturnsError) {
  v2::GetValuesRequest request;
  TextFormat::ParseFromString(R"pb(
                                metadata {
                                  fields {
                                    key: "hostname"
                                    value { string_value: "example.com" }
                                  }
                                }
                              )pb",
                              &request);

  v2::GetValuesResponse response;
  ExecutionMetadata unused_execution_metadata;
  SinglePartitionProcessor processor(*request_context_factory_,
                                     mock_udf_client_);
  const auto status =
      processor.Process(request, response, unused_execution_metadata);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace kv_server
