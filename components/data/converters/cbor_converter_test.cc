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

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "public/query/v2/get_values_v2.pb.h"

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
    "partitions": [
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

  V2CompressionGroup compression_group;
  TextFormat::ParseFromString(
      R"pb(partitions {
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
           partitions {
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
    "partitions": [
      {
        "id": 0,
        "keyGroupOutputs": []
      }
    ]
  }
)"_json;

  V2CompressionGroup compression_group;
  TextFormat::ParseFromString(R"pb(partitions { id: 0 })pb",
                              &compression_group);
  absl::StatusOr<std::string> cbor_encoded_proto_maybe =
      V2CompressionGroupCborEncode(compression_group);
  ASSERT_TRUE(cbor_encoded_proto_maybe.ok());
  ASSERT_TRUE(json_etalon == json::from_cbor(*cbor_encoded_proto_maybe));
}

}  // namespace
}  // namespace kv_server
