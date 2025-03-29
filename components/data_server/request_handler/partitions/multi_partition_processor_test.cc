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

#include "components/data_server/request_handler/partitions/multi_partition_processor.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "components/data_server/request_handler/content_type/mocks.h"
#include "components/udf/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "public/constants.h"
#include "public/test_util/proto_matcher.h"
#include "public/test_util/request_example.h"

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

class MultiPartitionProcessorTest
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

  std::unique_ptr<RequestContextFactory> request_context_factory_;
  MockUdfClient mock_udf_client_;
  MockV2EncoderDecoder mock_v2_codec_;
};

INSTANTIATE_TEST_SUITE_P(
    MultiPartitionProcessorTest, MultiPartitionProcessorTest,
    testing::Values(
        TestingParameters{
            .request_json = kv_server::kV2RequestMultiplePartitionsInJson,
        },
        TestingParameters{
            .request_json =
                kv_server::kConsentedV2RequestMultiplePartitionsInJson,
        }));

TEST_P(MultiPartitionProcessorTest, Success) {
  UDFExecutionMetadata udf_metadata;
  TextFormat::ParseFromString(kExampleV2MultiPartitionUdfMetadata,
                              &udf_metadata);

  UDFArgument arg1, arg2, arg3;
  TextFormat::ParseFromString(kExampleV2MultiPartitionUdfArg1, &arg1);
  TextFormat::ParseFromString(kExampleV2MultiPartitionUdfArg2, &arg2);
  TextFormat::ParseFromString(kExampleV2MultiPartitionUdfArg3, &arg3);
  nlohmann::json output1 = nlohmann::json::parse(R"(
  {
    "keyGroupOutputs": [
        {
            "keyValues": {
                "hello": {
                  "value": "world"
                }
            },
            "tags": [
                "structured",
                "groupNames"
            ]
        }
    ]
  }
    )");
  nlohmann::json output2 = nlohmann::json::parse(R"(
  {
    "keyGroupOutputs": [
        {
            "keyValues": {
                "key1": {
                  "value": "value1"
                }
            },
            "tags": [
                "custom",
                "keys"
            ]
        }
    ]
  }
    )");
  nlohmann::json output3 = nlohmann::json::parse(R"(
  {
    "keyGroupOutputs": [
        {
             "keyValues": {
                "key2": {
                  "value": "value2"
                }
            },
            "tags": [
                "custom",
                "keys"
            ]
        }
    ]
  }
    )");
  absl::flat_hash_map<UniquePartitionIdTuple, std::string>
      batch_execute_output = {{{0, 0}, output1.dump()},
                              {{0, 1}, output2.dump()},
                              {{2, 0}, output3.dump()}};

  EXPECT_CALL(
      mock_udf_client_,
      BatchExecuteCode(
          _,
          testing::UnorderedElementsAre(
              testing::Pair(
                  UniquePartitionIdTuple({0, 0}),
                  testing::FieldsAre(EqualsProto(udf_metadata),
                                     testing::ElementsAre(EqualsProto(arg1)))),
              testing::Pair(
                  UniquePartitionIdTuple({0, 1}),
                  testing::FieldsAre(EqualsProto(udf_metadata),
                                     testing::ElementsAre(EqualsProto(arg2)))),
              testing::Pair(
                  UniquePartitionIdTuple({2, 0}),
                  testing::FieldsAre(EqualsProto(udf_metadata),
                                     testing::ElementsAre(EqualsProto(arg3))))),
          _))
      .WillOnce(Return(batch_execute_output));

  EXPECT_CALL(mock_v2_codec_,
              EncodePartitionOutputs(testing::UnorderedElementsAre(
                                         testing::Pair(0, output1.dump()),
                                         testing::Pair(2, output3.dump())),
                                     _))
      .WillOnce(Return("compression_group_0_content"));

  EXPECT_CALL(
      mock_v2_codec_,
      EncodePartitionOutputs(
          testing::UnorderedElementsAre(testing::Pair(0, output2.dump())), _))
      .WillOnce(Return("compression_group_1_content"));

  const auto request = GetTestRequestBody();
  v2::GetValuesResponse response;
  ExecutionMetadata unused_execution_metadata;
  MultiPartitionProcessor processor(*request_context_factory_, mock_udf_client_,
                                    mock_v2_codec_);
  const auto status =
      processor.Process(request, response, unused_execution_metadata);
  ASSERT_TRUE(status.ok()) << status;

  v2::GetValuesResponse expected_response;
  EXPECT_TRUE(
      TextFormat::ParseFromString(R"pb(
                                    compression_groups {
                                      compression_group_id: 0
                                      content: "compression_group_0_content"
                                    }
                                    compression_groups {
                                      compression_group_id: 1
                                      content: "compression_group_1_content"
                                    }
                                  )pb",
                                  &expected_response));
  std::vector<v2::CompressionGroup> expected_compression_groups(
      expected_response.compression_groups().begin(),
      expected_response.compression_groups().end());
  std::vector<v2::CompressionGroup> actual_compression_groups(
      response.compression_groups().begin(),
      response.compression_groups().end());
  for (const auto& expected_compression_group : expected_compression_groups) {
    EXPECT_THAT(actual_compression_groups,
                testing::Contains(EqualsProto(expected_compression_group)));
  }
}

TEST_P(MultiPartitionProcessorTest, IgnoreFailedUdfCompressionGroup) {
  nlohmann::json output2 = nlohmann::json::parse(R"(
  {
    "keyGroupOutputs": [
        {
            "keyValues": {
                "key1": {
                  "value": "value1"
                }
            },
            "tags": [
                "custom",
                "keys"
            ]
        }
    ]
  }
    )");
  absl::flat_hash_map<UniquePartitionIdTuple, std::string>
      batch_execute_output = {{{0, 1}, output2.dump()}};

  EXPECT_CALL(mock_udf_client_, BatchExecuteCode(_, _, _))
      .WillOnce(Return(batch_execute_output));

  EXPECT_CALL(
      mock_v2_codec_,
      EncodePartitionOutputs(
          testing::UnorderedElementsAre(testing::Pair(0, output2.dump())), _))
      .WillOnce(Return("compression_group_1_content"));

  const auto request = GetTestRequestBody();
  v2::GetValuesResponse response;
  ExecutionMetadata unused_execution_metadata;
  MultiPartitionProcessor processor(*request_context_factory_, mock_udf_client_,
                                    mock_v2_codec_);
  const auto status =
      processor.Process(request, response, unused_execution_metadata);
  ASSERT_TRUE(status.ok()) << status;

  v2::GetValuesResponse expected_response;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        compression_groups {
          compression_group_id: 1
          content: "compression_group_1_content"
        })pb",
      &expected_response));
  EXPECT_THAT(response, EqualsProto(expected_response));
}

TEST_P(MultiPartitionProcessorTest, IgnoreFailedUdfPartition) {
  nlohmann::json output1 = nlohmann::json::parse(R"(
  {
    "keyGroupOutputs": [
        {
            "keyValues": {
                "hello": {
                  "value": "world"
                }
            },
            "tags": [
                "structured",
                "groupNames"
            ]
        }
    ]
  }
    )");
  nlohmann::json output2 = nlohmann::json::parse(R"(
  {
    "keyGroupOutputs": [
        {
            "keyValues": {
                "key1": {
                  "value": "value1"
                }
            },
            "tags": [
                "custom",
                "keys"
            ]
        }
    ]
  }
    )");
  absl::flat_hash_map<UniquePartitionIdTuple, std::string>
      batch_execute_output = {{{0, 0}, output1.dump()},
                              {{0, 1}, output2.dump()}};

  EXPECT_CALL(mock_udf_client_, BatchExecuteCode(_, _, _))
      .WillOnce(Return(batch_execute_output));

  EXPECT_CALL(
      mock_v2_codec_,
      EncodePartitionOutputs(
          testing::UnorderedElementsAre(testing::Pair(0, output1.dump())), _))
      .WillOnce(Return("compression_group_0_content"));

  EXPECT_CALL(
      mock_v2_codec_,
      EncodePartitionOutputs(
          testing::UnorderedElementsAre(testing::Pair(0, output2.dump())), _))
      .WillOnce(Return("compression_group_1_content"));

  const auto request = GetTestRequestBody();
  v2::GetValuesResponse response;
  ExecutionMetadata unused_execution_metadata;
  MultiPartitionProcessor processor(*request_context_factory_, mock_udf_client_,
                                    mock_v2_codec_);
  const auto status =
      processor.Process(request, response, unused_execution_metadata);
  ASSERT_TRUE(status.ok()) << status;

  v2::GetValuesResponse expected_response;
  EXPECT_TRUE(
      TextFormat::ParseFromString(R"pb(
                                    compression_groups {
                                      compression_group_id: 0
                                      content: "compression_group_0_content"
                                    }
                                    compression_groups {
                                      compression_group_id: 1
                                      content: "compression_group_1_content"
                                    }
                                  )pb",
                                  &expected_response));
  std::vector<v2::CompressionGroup> expected_compression_groups(
      expected_response.compression_groups().begin(),
      expected_response.compression_groups().end());
  std::vector<v2::CompressionGroup> actual_compression_groups(
      response.compression_groups().begin(),
      response.compression_groups().end());
  for (const auto& expected_compression_group : expected_compression_groups) {
    EXPECT_THAT(actual_compression_groups,
                testing::Contains(EqualsProto(expected_compression_group)));
  }
}

TEST_P(MultiPartitionProcessorTest, ReturnErrorWhenAllUdfExecutionsFail) {
  EXPECT_CALL(mock_udf_client_, BatchExecuteCode(_, _, _))
      .WillOnce(Return(absl::InternalError("Batch UDF execution error")));

  const auto request = GetTestRequestBody();
  v2::GetValuesResponse response;
  ExecutionMetadata unused_execution_metadata;
  MultiPartitionProcessor processor(*request_context_factory_, mock_udf_client_,
                                    mock_v2_codec_);
  const auto status =
      processor.Process(request, response, unused_execution_metadata);
  ASSERT_FALSE(status.ok()) << status;
  v2::GetValuesResponse expected_response;
  EXPECT_THAT(response, EqualsProto(expected_response));
}

TEST_P(MultiPartitionProcessorTest, IgnoreEncodeErrorForOneCompressionGroup) {
  absl::flat_hash_map<UniquePartitionIdTuple, std::string>
      batch_execute_output = {{{0, 0}, "udf_output_1"},
                              {{0, 1}, "udf_output_2"},
                              {{2, 0}, "udf_output_3"}};

  EXPECT_CALL(mock_udf_client_, BatchExecuteCode(_, _, _))
      .WillOnce(Return(batch_execute_output));

  EXPECT_CALL(mock_v2_codec_,
              EncodePartitionOutputs(testing::UnorderedElementsAre(
                                         testing::Pair(0, "udf_output_1"),
                                         testing::Pair(2, "udf_output_3")),
                                     _))
      .WillOnce(
          Return(absl::InternalError("Error encoding partition outputs")));

  EXPECT_CALL(
      mock_v2_codec_,
      EncodePartitionOutputs(
          testing::UnorderedElementsAre(testing::Pair(0, "udf_output_2")), _))
      .WillOnce(Return("compression_group_1_content"));

  const auto request = GetTestRequestBody();
  v2::GetValuesResponse response;
  ExecutionMetadata unused_execution_metadata;
  MultiPartitionProcessor processor(*request_context_factory_, mock_udf_client_,
                                    mock_v2_codec_);
  const auto status =
      processor.Process(request, response, unused_execution_metadata);
  ASSERT_TRUE(status.ok()) << status;

  v2::GetValuesResponse expected_response;
  EXPECT_TRUE(
      TextFormat::ParseFromString(R"pb(
                                    compression_groups {
                                      compression_group_id: 1
                                      content: "compression_group_1_content"
                                    }
                                  )pb",
                                  &expected_response));
  EXPECT_THAT(response, EqualsProto(expected_response));
}

TEST_P(MultiPartitionProcessorTest,
       ReturnErrorWhenAllCompressionGroupEncodingsFail) {
  absl::flat_hash_map<UniquePartitionIdTuple, std::string>
      batch_execute_output = {{{0, 0}, "udf_output_1"},
                              {{0, 1}, "udf_output_2"},
                              {{2, 0}, "udf_output_3"}};

  EXPECT_CALL(mock_udf_client_, BatchExecuteCode(_, _, _))
      .WillOnce(Return(batch_execute_output));

  EXPECT_CALL(mock_v2_codec_, EncodePartitionOutputs(_, _))
      .Times(2)
      .WillRepeatedly(
          Return(absl::InternalError("Error encoding partition outputs")));

  const auto request = GetTestRequestBody();
  v2::GetValuesResponse response;
  ExecutionMetadata unused_execution_metadata;
  MultiPartitionProcessor processor(*request_context_factory_, mock_udf_client_,
                                    mock_v2_codec_);
  const auto status =
      processor.Process(request, response, unused_execution_metadata);
  ASSERT_FALSE(status.ok());
  v2::GetValuesResponse expected_response;
  EXPECT_THAT(response, EqualsProto(expected_response));
}

TEST_F(MultiPartitionProcessorTest, ProcessesPartitionMetadataCorrectly) {
  v2::GetValuesRequest request;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        metadata {
          fields {
            key: "request_level_metadata"
            value { string_value: "request_level_value" }
          }
        }
        partitions {
          id: 0
          compression_group_id: 0
          metadata {
            fields {
              key: "partition_00_metadata"
              value { string_value: "partition_00_value" }
            }
          }
        }
        partitions {
          id: 1
          compression_group_id: 0
          metadata {
            fields {
              key: "partition_10_metadata"
              value { string_value: "partition_10_value" }
            }
          }
        }
      )pb",
      &request));
  UDFExecutionMetadata udf_metadata1;
  TextFormat::ParseFromString(
      R"pb(request_metadata {
             fields {
               key: "request_level_metadata"
               value { string_value: "request_level_value" }
             }
           }
           partition_metadata {
             fields {
               key: "partition_00_metadata"
               value { string_value: "partition_00_value" }
             }
           }
      )pb",
      &udf_metadata1);
  UDFExecutionMetadata udf_metadata2;
  TextFormat::ParseFromString(
      R"pb(request_metadata {
             fields {
               key: "request_level_metadata"
               value { string_value: "request_level_value" }
             }
           }
           partition_metadata {
             fields {
               key: "partition_10_metadata"
               value { string_value: "partition_10_value" }
             }
           }
      )pb",
      &udf_metadata2);

  UDFArgument arg;

  absl::flat_hash_map<UniquePartitionIdTuple, std::string>
      batch_execute_output = {{{0, 0}, "udf_output_1"},
                              {{1, 0}, "udf_output_2"}};

  EXPECT_CALL(
      mock_udf_client_,
      BatchExecuteCode(
          _,
          testing::UnorderedElementsAre(
              testing::Pair(UniquePartitionIdTuple({0, 0}),
                            testing::FieldsAre(EqualsProto(udf_metadata1),
                                               testing::IsEmpty())),
              testing::Pair(UniquePartitionIdTuple({1, 0}),
                            testing::FieldsAre(EqualsProto(udf_metadata2),
                                               testing::IsEmpty()))),
          _))
      .WillOnce(Return(batch_execute_output));

  EXPECT_CALL(mock_v2_codec_, EncodePartitionOutputs(_, _))
      .WillOnce(Return("compression_group_content"));

  v2::GetValuesResponse response;
  ExecutionMetadata unused_execution_metadata;
  MultiPartitionProcessor processor(*request_context_factory_, mock_udf_client_,
                                    mock_v2_codec_);
  const auto status =
      processor.Process(request, response, unused_execution_metadata);
  ASSERT_TRUE(status.ok()) << status;
  v2::GetValuesResponse expected_response;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        compression_groups {
          compression_group_id: 0
          content: "compression_group_content"
        })pb",
      &expected_response));
  EXPECT_THAT(response, EqualsProto(expected_response));
}

}  // namespace
}  // namespace kv_server
