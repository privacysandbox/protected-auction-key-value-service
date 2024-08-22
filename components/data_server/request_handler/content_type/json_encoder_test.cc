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

#include "components/data_server/request_handler/content_type/json_encoder.h"

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

TEST(JsonEncoderTest, EncodeV2GetValuesResponseCompressionGroupSuccess) {
  // "abc" -> base64 encode => YWJj
  json expected = R"({
    "compressionGroups": [
      {
        "compressionGroupId": 1,
        "content": "YWJj",
        "ttlMs": 3
      }
    ]
  })"_json;

  v2::GetValuesResponse response_proto;
  TextFormat::ParseFromString(
      R"pb(
        compression_groups { compression_group_id: 1 content: "abc" ttl_ms: 3 }
      )pb",
      &response_proto);

  JsonV2EncoderDecoder encoder;
  const auto maybe_json_response =
      encoder.EncodeV2GetValuesResponse(response_proto);
  ASSERT_TRUE(maybe_json_response.ok()) << maybe_json_response.status();
  nlohmann::json json_response = nlohmann::json::parse(*maybe_json_response);
  EXPECT_EQ(expected, json_response);
}

TEST(JsonEncoderTest, EncodeV2GetValuesResponseSinglePartitionSuccess) {
  json expected = R"({
    "singlePartition": { "stringOutput": "abc" }
  })"_json;

  v2::GetValuesResponse response_proto;
  TextFormat::ParseFromString(
      R"pb(
        single_partition { string_output: "abc" }
      )pb",
      &response_proto);

  JsonV2EncoderDecoder encoder;
  const auto maybe_json_response =
      encoder.EncodeV2GetValuesResponse(response_proto);
  ASSERT_TRUE(maybe_json_response.ok()) << maybe_json_response.status();
  EXPECT_EQ(expected.dump(), *maybe_json_response);
}

TEST(JsonEncoderTest, EncodePartitionOutputsSuccess) {
  InitMetricsContextMap();
  json json_partition_output1 = R"(
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
      }
  )"_json;
  json json_partition_output2 = "stringOutput";
  std::vector<std::string> partition_output_strings = {
      json_partition_output1.dump(), json_partition_output2.dump()};

  auto request_context_factory = std::make_unique<RequestContextFactory>();
  JsonV2EncoderDecoder encoder;
  const auto maybe_json_content = encoder.EncodePartitionOutputs(
      partition_output_strings, *request_context_factory);

  json expected = {json_partition_output1, json_partition_output2};
  ASSERT_TRUE(maybe_json_content.ok()) << maybe_json_content.status();
  EXPECT_EQ(expected.dump(), *maybe_json_content);
}

TEST(JsonEncoderTest, EncodePartitionOutputsEmptyFails) {
  InitMetricsContextMap();
  std::vector<std::string> partition_output_strings = {};

  std::string content;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  JsonV2EncoderDecoder encoder;
  const auto maybe_json_content = encoder.EncodePartitionOutputs(
      partition_output_strings, *request_context_factory);

  ASSERT_FALSE(maybe_json_content.ok()) << maybe_json_content.status();
}

TEST(JsonEncoderTest, DecodeToV2GetValuesRequestProtoEmptyStringFailure) {
  std::string request = "";
  JsonV2EncoderDecoder encoder;
  const auto maybe_request = encoder.DecodeToV2GetValuesRequestProto(request);
  ASSERT_FALSE(maybe_request.ok()) << maybe_request.status();
}

TEST(JsonEncoderTest, DecodeToV2GetValuesRequestSuccess) {
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
  JsonV2EncoderDecoder encoder;
  const auto maybe_request =
      encoder.DecodeToV2GetValuesRequestProto(json_message.dump());
  ASSERT_TRUE(maybe_request.ok()) << maybe_request.status();
  EXPECT_THAT(expected, EqualsProto(*maybe_request));
}

}  // namespace
}  // namespace kv_server
