// Copyright 2024 Google LLC
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

#include "components/data_server/request_handler/content_type/cbor_encoder.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "public/test_util/proto_matcher.h"

namespace kv_server {
namespace {

using json = nlohmann::json;
using google::protobuf::TextFormat;

TEST(CborEncoderTest, EncodeV2GetValuesResponseCompressionGroupSuccess) {
  // "abc" -> [97,98,99] as byte array
  json json_v2_response = R"({
    "compressionGroups": [
      {
        "compressionGroupId": 1,
        "content": {"bytes":[97,98,99],"subtype":null},
        "ttlMs": 2
      }
    ]
  })"_json;

  v2::GetValuesResponse response_proto;
  TextFormat::ParseFromString(
      R"pb(
        compression_groups { compression_group_id: 1 ttl_ms: 2 content: "abc" }
      )pb",
      &response_proto);

  CborV2EncoderDecoder encoder;
  const auto maybe_cbor_response =
      encoder.EncodeV2GetValuesResponse(response_proto);
  ASSERT_TRUE(maybe_cbor_response.ok()) << maybe_cbor_response.status();
  EXPECT_EQ(json_v2_response.dump(),
            json::from_cbor(*maybe_cbor_response).dump());
}

TEST(CborEncoderTest, EncodeV2GetValuesResponseSinglePartitionFailure) {
  v2::GetValuesResponse response_proto;
  TextFormat::ParseFromString(
      R"pb(
        single_partition { string_output: "abc" }
      )pb",
      &response_proto);

  CborV2EncoderDecoder encoder;
  const auto maybe_cbor_response =
      encoder.EncodeV2GetValuesResponse(response_proto);
  ASSERT_FALSE(maybe_cbor_response.ok()) << maybe_cbor_response.status();
  EXPECT_EQ(maybe_cbor_response.status().message(),
            "single_partition is not supported for cbor content type");
}

TEST(CborEncoderTest, EncodePartitionOutputsSuccess) {
  InitMetricsContextMap();
  json json_partition_output1 = R"(
      {
        "keyGroupOutputs": [
          {
            "keyValues": {
              "hello": {
                "value": "world"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
          }
        ]
      })"_json;
  json json_partition_output2 = R"(
      {
        "keyGroupOutputs": [
          {
            "keyValues": {
              "hello2": {
                "value": "world2"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
          }
        ]
      }
  )"_json;
  std::vector<std::pair<int32_t, std::string>> partition_output_pairs = {
      {1, json_partition_output1.dump()}, {2, json_partition_output2.dump()}};

  auto request_context_factory = std::make_unique<RequestContextFactory>();
  CborV2EncoderDecoder encoder;
  const auto maybe_cbor_content = encoder.EncodePartitionOutputs(
      partition_output_pairs, *request_context_factory);

  json expected_partition_output1 = {{"id", 1}};
  expected_partition_output1.update(json_partition_output1);
  json expected_partition_output2 = {{"id", 2}};
  expected_partition_output2.update(json_partition_output2);
  json expected_partition_outputs = {expected_partition_output1,
                                     expected_partition_output2};
  ASSERT_TRUE(maybe_cbor_content.ok()) << maybe_cbor_content.status();
  EXPECT_EQ(expected_partition_outputs, json::from_cbor(*maybe_cbor_content));
}

TEST(CborEncoderTest,
     EncodePartitionOutputsIgnoresKeyGroupOutputWithoutKVsSuccess) {
  InitMetricsContextMap();
  json json_partition_output1 = R"(
      {
        "keyGroupOutputs": [
          {
            "keyValues": {
            },
            "tags": [
              "custom",
              "keys"
            ]
          }
        ]
      })"_json;
  json json_partition_output2 = R"(
      {
        "keyGroupOutputs": [
          {
            "tags": [
              "custom",
              "keys"
            ]
          }
        ]
      })"_json;
  std::vector<std::pair<int32_t, std::string>> partition_output_pairs = {
      {1, json_partition_output1.dump()}, {2, json_partition_output2.dump()}};

  auto request_context_factory = std::make_unique<RequestContextFactory>();
  CborV2EncoderDecoder encoder;
  const auto maybe_cbor_content = encoder.EncodePartitionOutputs(
      partition_output_pairs, *request_context_factory);

  json expected_empty_json_output = R"(
      {
        "keyGroupOutputs": []
      })"_json;

  json expected_partition_output1 = {{"id", 1}};
  expected_partition_output1.update(expected_empty_json_output);
  json expected_partition_output2 = {{"id", 2}};
  expected_partition_output2.update(expected_empty_json_output);
  json expected_partition_outputs = {expected_partition_output1,
                                     expected_partition_output2};
  ASSERT_TRUE(maybe_cbor_content.ok()) << maybe_cbor_content.status();
  EXPECT_EQ(expected_partition_outputs, json::from_cbor(*maybe_cbor_content));
}

TEST(CborEncoderTest, EncodePartitionOutputsEmptyKeyGroupOutputSuccess) {
  InitMetricsContextMap();
  json json_partition_output = R"(
      {
        "keyGroupOutputs": []
      })"_json;
  std::vector<std::pair<int32_t, std::string>> partition_output_pairs = {
      {1, json_partition_output.dump()}};

  std::string content;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  CborV2EncoderDecoder encoder;
  const auto maybe_cbor_content = encoder.EncodePartitionOutputs(
      partition_output_pairs, *request_context_factory);

  json expected_json_outputs = R"([
      {
        "id": 1,
        "keyGroupOutputs": []
      }])"_json;
  ASSERT_TRUE(maybe_cbor_content.ok()) << maybe_cbor_content.status();
  EXPECT_EQ(expected_json_outputs, json::from_cbor(*maybe_cbor_content));
}

TEST(CborEncoderTest, EncodePartitionOutputs_OverwritesId) {
  InitMetricsContextMap();
  json json_partition_output = R"(
      {
        "id": 100,
        "keyGroupOutputs": []
      })"_json;
  std::vector<std::pair<int32_t, std::string>> partition_output_pairs = {
      {1, json_partition_output.dump()}};

  std::string content;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  CborV2EncoderDecoder encoder;
  const auto maybe_cbor_content = encoder.EncodePartitionOutputs(
      partition_output_pairs, *request_context_factory);

  json expected_json_outputs = R"([
      {
        "id": 1,
        "keyGroupOutputs": []
      }])"_json;
  ASSERT_TRUE(maybe_cbor_content.ok()) << maybe_cbor_content.status();
  EXPECT_EQ(expected_json_outputs, json::from_cbor(*maybe_cbor_content));
}

TEST(CborEncoderTest, EncodePartitionOutputsInvalidPartitionOutputIgnored) {
  InitMetricsContextMap();
  json json_partition_output_invalid = R"(
      {
        "keyGroupOtputs": []
      }
  )"_json;

  json json_partition_output_valid = R"(
      {
        "keyGroupOutputs": []
      }
  )"_json;
  std::vector<std::pair<int32_t, std::string>> partition_output_pairs = {
      {1, json_partition_output_invalid.dump()},
      {2, json_partition_output_valid.dump()}};

  auto request_context_factory = std::make_unique<RequestContextFactory>();
  CborV2EncoderDecoder encoder;
  const auto maybe_cbor_content = encoder.EncodePartitionOutputs(
      partition_output_pairs, *request_context_factory);

  ASSERT_TRUE(maybe_cbor_content.ok()) << maybe_cbor_content.status();
  json partition_outputs_json = json::array();
  json expected_partition_output_valid = {{"id", 2}};
  expected_partition_output_valid.update(json_partition_output_valid);
  partition_outputs_json.emplace_back(expected_partition_output_valid);
  EXPECT_EQ(partition_outputs_json, json::from_cbor(*maybe_cbor_content));
}

TEST(CborEncoderTest, EncodePartitionOutputsAllInvalidPartitionOutputFails) {
  InitMetricsContextMap();
  json json_partition_output_invalid = R"(
      {
        "keyGroupOtputs": []
      }
  )"_json;
  std::vector<std::pair<int32_t, std::string>> partition_output_pairs = {
      {1, json_partition_output_invalid.dump()}};

  std::string content;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  CborV2EncoderDecoder encoder;
  const auto maybe_cbor_content = encoder.EncodePartitionOutputs(
      partition_output_pairs, *request_context_factory);

  ASSERT_FALSE(maybe_cbor_content.ok()) << maybe_cbor_content.status();
}

TEST(CborEncoderTest,
     EncodePartitionOutputsNonJsonObjectReturnedByUdfDoesntCrashServer) {
  InitMetricsContextMap();
  std::vector<std::pair<int32_t, std::string>> partition_output_pairs = {
      {1, "\"json_string_not_object\""}};
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  CborV2EncoderDecoder encoder;
  const auto maybe_cbor_content = encoder.EncodePartitionOutputs(
      partition_output_pairs, *request_context_factory);
  ASSERT_FALSE(maybe_cbor_content.ok()) << maybe_cbor_content.status();
}

TEST(CborEncoderTest, DecodeToV2GetValuesRequestProtoEmptyStringSuccess) {
  std::string request = "";
  CborV2EncoderDecoder encoder;
  const auto maybe_request = encoder.DecodeToV2GetValuesRequestProto(request);
  ASSERT_FALSE(maybe_request.ok()) << maybe_request.status();
}

TEST(CborEncoderTest, DecodeToV2GetValuesRequestSuccess) {
  v2::GetValuesRequest expected;
  TextFormat::ParseFromString(R"pb(
                                client_version: "version1"
                                metadata {
                                  fields {
                                    key: "foo"
                                    value { string_value: "bar1" }
                                  }
                                }
                                partitions {
                                  id: 1
                                  compression_group_id: 1
                                  metadata {
                                    fields {
                                      key: "partition_metadata"
                                      value { string_value: "bar2" }
                                    }
                                  }
                                  arguments {
                                    tags {
                                      values { string_value: "tag1" }
                                      values { string_value: "tag2" }
                                    }

                                    data { string_value: "bar4" }
                                  }
                                }
                              )pb",
                              &expected);

  nlohmann::json json_message = R"(
 {
    "clientVersion": "version1",
    "metadata": {
        "foo": "bar1"
    },
    "partitions": [
        {
            "id": 1,
            "compressionGroupId": 1,
            "metadata": {
                "partition_metadata": "bar2"
            },
            "arguments": {
                "tags": [
                    "tag1",
                    "tag2"
                ],
                "data": "bar4"
            }
        }
    ]
}
)"_json;
  std::vector<std::uint8_t> v = json::to_cbor(json_message);
  std::string cbor_raw(v.begin(), v.end());
  CborV2EncoderDecoder encoder;
  const auto maybe_request = encoder.DecodeToV2GetValuesRequestProto(cbor_raw);
  ASSERT_TRUE(maybe_request.ok()) << maybe_request.status();
  EXPECT_THAT(expected, EqualsProto(*maybe_request));
}

}  // namespace
}  // namespace kv_server
