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

#include "components/data_server/request_handler/content_type/proto_encoder.h"

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

using google::protobuf::TextFormat;
using nlohmann::json;

TEST(ProtoEncoderTest, EncodeV2GetValuesResponseSuccess) {
  v2::GetValuesResponse response_proto;
  TextFormat::ParseFromString(
      R"pb(
        compression_groups { compression_group_id: 1 content: "abc" ttl_ms: 3 }
        single_partition { string_output: "abc" }
      )pb",
      &response_proto);
  ProtoV2EncoderDecoder encoder;
  const auto maybe_proto_response =
      encoder.EncodeV2GetValuesResponse(response_proto);
  ASSERT_TRUE(maybe_proto_response.ok()) << maybe_proto_response.status();
  std::string expected;
  response_proto.SerializeToString(&expected);
  EXPECT_EQ(expected, *maybe_proto_response);
}

TEST(ProtoEncoderTest, EncodePartitionOutputsSuccess) {
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
  ProtoV2EncoderDecoder encoder;
  const auto maybe_proto_content = encoder.EncodePartitionOutputs(
      partition_output_pairs, *request_context_factory);

  json expected_output1 = {{"id", 1}};
  expected_output1.update(json_partition_output1);
  json expected_output2 = {{"id", 2}};
  expected_output2.update(json_partition_output2);
  json expected = {expected_output1, expected_output2};
  ASSERT_TRUE(maybe_proto_content.ok()) << maybe_proto_content.status();
  EXPECT_EQ(expected.dump(), *maybe_proto_content);
}

TEST(JsonEncoderTest, EncodePartitionOutputsEmptyFails) {
  InitMetricsContextMap();
  std::vector<std::pair<int32_t, std::string>> partition_output_pairs = {};
  std::string content;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  ProtoV2EncoderDecoder encoder;
  const auto maybe_proto_content = encoder.EncodePartitionOutputs(
      partition_output_pairs, *request_context_factory);

  ASSERT_FALSE(maybe_proto_content.ok()) << maybe_proto_content.status();
}

TEST(ProtoEncoderTest, DecodeToV2GetValuesRequestProtoEmptyStringFailure) {
  std::string request = "";
  ProtoV2EncoderDecoder encoder;
  const auto maybe_request = encoder.DecodeToV2GetValuesRequestProto(request);
  ASSERT_FALSE(maybe_request.ok()) << maybe_request.status();
}

TEST(ProtoEncoderTest, DecodeToV2GetValuesRequestSuccess) {
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
  ProtoV2EncoderDecoder encoder;
  std::string serialized_request;
  expected.SerializeToString(&serialized_request);
  const auto maybe_request =
      encoder.DecodeToV2GetValuesRequestProto(serialized_request);
  ASSERT_TRUE(maybe_request.ok()) << maybe_request.status();
  EXPECT_THAT(expected, EqualsProto(*maybe_request));
}

}  // namespace
}  // namespace kv_server
