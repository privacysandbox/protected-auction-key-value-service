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
  json json_partition_outputs = R"(
    [
      {
        "id": 0,
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
      },
      {
        "id": 1,
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
    ]
)"_json;
  std::vector<std::string> partition_output_strings = {
      json_partition_outputs[0].dump(), json_partition_outputs[1].dump()};

  auto request_context_factory = std::make_unique<RequestContextFactory>();
  CborV2EncoderDecoder encoder;
  const auto maybe_cbor_content = encoder.EncodePartitionOutputs(
      partition_output_strings, *request_context_factory);

  ASSERT_TRUE(maybe_cbor_content.ok()) << maybe_cbor_content.status();
  EXPECT_EQ(json_partition_outputs, json::from_cbor(*maybe_cbor_content));
}

TEST(CborEncoderTest, EncodePartitionOutputsEmptyKeyGroupOutputSuccess) {
  InitMetricsContextMap();
  json json_partition_outputs = R"(
   [
      {
        "id": 0,
        "keyGroupOutputs": []
      }
    ]
)"_json;
  std::vector<std::string> partition_output_strings = {
      json_partition_outputs[0].dump()};

  std::string content;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  CborV2EncoderDecoder encoder;
  const auto maybe_cbor_content = encoder.EncodePartitionOutputs(
      partition_output_strings, *request_context_factory);

  ASSERT_TRUE(maybe_cbor_content.ok()) << maybe_cbor_content.status();
  EXPECT_EQ(json_partition_outputs, json::from_cbor(*maybe_cbor_content));
}

TEST(CborEncoderTest, EncodePartitionOutputsInvalidPartitionOutputIgnored) {
  InitMetricsContextMap();
  json json_partition_output_invalid = R"(
      {
        "id": 0,
        "keyGroupOtputs": []
      }
  )"_json;

  json json_partition_output_valid = R"(
      {
        "id": 0,
        "keyGroupOutputs": []
      }
  )"_json;
  std::vector<std::string> partition_output_strings = {
      json_partition_output_invalid.dump(), json_partition_output_valid.dump()};

  auto request_context_factory = std::make_unique<RequestContextFactory>();
  CborV2EncoderDecoder encoder;
  const auto maybe_cbor_content = encoder.EncodePartitionOutputs(
      partition_output_strings, *request_context_factory);

  ASSERT_TRUE(maybe_cbor_content.ok()) << maybe_cbor_content.status();
  json partition_outputs_json = json::array();
  partition_outputs_json.emplace_back(json_partition_output_valid);
  EXPECT_EQ(partition_outputs_json, json::from_cbor(*maybe_cbor_content));
}

TEST(CborEncoderTest, EncodePartitionOutputsAllInvalidPartitionOutputFails) {
  InitMetricsContextMap();
  json json_partition_output_invalid = R"(
      {
        "id": 0,
        "keyGroupOtputs": []
      }
  )"_json;
  std::vector<std::string> partition_output_strings = {
      json_partition_output_invalid.dump()};

  std::string content;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  CborV2EncoderDecoder encoder;
  const auto maybe_cbor_content = encoder.EncodePartitionOutputs(
      partition_output_strings, *request_context_factory);

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
