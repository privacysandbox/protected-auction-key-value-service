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

#include "components/data/converters/cbor_converter.h"

#include <vector>

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "public/query/v2/get_values_v2.pb.h"
#include "public/test_util/proto_matcher.h"

namespace kv_server {
namespace {

using json = nlohmann::json;
using google::protobuf::TextFormat;

TEST(CborConverterTest, V2GetValuesResponseCborEncodeSuccess) {
  json json_etalon = R"({
    "compressionGroups": [
      {
        "compressionGroupId": 1,
        "content": "abc",
        "ttlMs": 2
      }
    ]
  })"_json;

  v2::GetValuesResponse response;
  TextFormat::ParseFromString(
      R"pb(
        compression_groups { compression_group_id: 1 ttl_ms: 2 content: "abc" }
      )pb",
      &response);
  absl::StatusOr<std::string> cbor_encoded_proto_maybe =
      V2GetValuesResponseCborEncode(response);
  EXPECT_TRUE(cbor_encoded_proto_maybe.ok());
  EXPECT_TRUE(json_etalon == json::from_cbor(*cbor_encoded_proto_maybe));
}

TEST(CborConverterTest, V2GetValuesResponseCborEncodeArrayMsSuccess) {
  json json_etalon = R"({
    "compressionGroups": [
      {
        "compressionGroupId": 1,
        "content": "abc"
      },
      {
        "compressionGroupId": 2,
        "content": "abcd",
        "ttlMs": 2
      }
    ]
  })"_json;

  v2::GetValuesResponse response;
  TextFormat::ParseFromString(
      R"pb(
        compression_groups { compression_group_id: 1 content: "abc" }
        compression_groups { compression_group_id: 2 ttl_ms: 2 content: "abcd" }
      )pb",
      &response);
  absl::StatusOr<std::string> cbor_encoded_proto_maybe =
      V2GetValuesResponseCborEncode(response);
  EXPECT_TRUE(cbor_encoded_proto_maybe.ok());
  EXPECT_TRUE(json_etalon == json::from_cbor(*cbor_encoded_proto_maybe));
}

TEST(CborConverterTest, V2CompressionGroupCborEncodeSuccess) {
  json json_etalon = R"(
  {
    "partitionOutputs": [
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
          },
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
  }
)"_json;

  application_pa::V2CompressionGroup compression_group;
  TextFormat::ParseFromString(
      R"pb(partition_outputs {
             id: 0
             key_group_outputs {
               tags: "custom"
               tags: "keys"
               key_values {
                 key: "hello"
                 value { value { string_value: "world" } }
               }
             }
             key_group_outputs {
               tags: "structured"
               tags: "groupNames"
               key_values {
                 key: "hello"
                 value { value { string_value: "world" } }
               }
             }
           }
           partition_outputs {
             id: 1
             key_group_outputs {
               tags: "custom"
               tags: "keys"
               key_values {
                 key: "hello2"
                 value { value { string_value: "world2" } }
               }
             }
           })pb",
      &compression_group);
  absl::StatusOr<std::string> cbor_encoded_proto_maybe =
      V2CompressionGroupCborEncode(compression_group);
  ASSERT_TRUE(cbor_encoded_proto_maybe.ok());
  ASSERT_TRUE(json_etalon == json::from_cbor(*cbor_encoded_proto_maybe));
}

TEST(CborConverterTest,
     V2CompressionGroupEmptyKeyGroupOutputsCborEncodeSuccess) {
  json json_etalon = R"(
  {
    "partitionOutputs": [
      {
        "id": 0,
        "keyGroupOutputs": []
      }
    ]
  }
)"_json;

  application_pa::V2CompressionGroup compression_group;
  TextFormat::ParseFromString(R"pb(partition_outputs { id: 0 })pb",
                              &compression_group);
  absl::StatusOr<std::string> cbor_encoded_proto_maybe =
      V2CompressionGroupCborEncode(compression_group);
  ASSERT_TRUE(cbor_encoded_proto_maybe.ok());
  ASSERT_TRUE(json_etalon == json::from_cbor(*cbor_encoded_proto_maybe));
}

TEST(CborConverterTest, CborDecodeToProtoSuccess) {
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
  ::kv_server::v2::GetValuesRequest actual;
  std::vector<std::uint8_t> v = json::to_cbor(json_message);
  std::string cbor_raw(v.begin(), v.end());
  const auto status = CborDecodeToProto(cbor_raw, actual);
  ASSERT_TRUE(status.ok());
  EXPECT_THAT(actual, EqualsProto(expected));
}

TEST(CborConverterTest, CborDecodeToProtoFailure) {
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
  ::kv_server::v2::GetValuesRequest actual;
  std::vector<std::uint8_t> v = json::to_cbor(json_message);
  std::string cbor_raw(v.begin(), --v.end());
  const auto status = CborDecodeToProto(cbor_raw, actual);
  ASSERT_FALSE(status.ok());
}

}  // namespace
}  // namespace kv_server
