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
using ordered_json = nlohmann::ordered_json;
using google::protobuf::TextFormat;

TEST(CborConverterTest, V2GetValuesResponseCborEncodeSuccess) {
  // "abc" -> [97,98,99] as byte array
  ordered_json json_etalon = nlohmann::ordered_json::parse(R"({
    "compressionGroups": [
      {
        "ttlMs": 2,
        "content": {"bytes":[97,98,99],"subtype":null},
        "compressionGroupId": 1
      }
    ]
  })");
  v2::GetValuesResponse response;
  TextFormat::ParseFromString(
      R"pb(
        compression_groups { compression_group_id: 1 ttl_ms: 2 content: "abc" }
      )pb",
      &response);
  absl::StatusOr<std::string> cbor_encoded_proto_maybe =
      V2GetValuesResponseCborEncode(response);
  ASSERT_TRUE(cbor_encoded_proto_maybe.ok())
      << cbor_encoded_proto_maybe.status();
  EXPECT_EQ(
      json_etalon.dump(),
      nlohmann::ordered_json::from_cbor(*cbor_encoded_proto_maybe).dump());
}

TEST(CborConverterTest, V2GetValuesResponseCborEncode_SinglePartition_Failure) {
  v2::GetValuesResponse response;
  TextFormat::ParseFromString(
      R"pb(
        single_partition {}
      )pb",
      &response);
  absl::StatusOr<std::string> cbor_encoded_proto_maybe =
      V2GetValuesResponseCborEncode(response);
  ASSERT_FALSE(cbor_encoded_proto_maybe.ok())
      << cbor_encoded_proto_maybe.status();
}

TEST(CborConverterTest, V2GetValuesResponseCborEncodeArrayMsSuccess) {
  ordered_json json_etalon = nlohmann::ordered_json::parse(R"({
    "compressionGroups": [
      {
        "content": {"bytes":[97,98,99], "subtype":null },
                "compressionGroupId": 1

      },
      {
        "ttlMs": 2,
        "content": {"bytes":[97,98,99,100], "subtype":null },
        "compressionGroupId": 2
      }
    ]
  })");

  v2::GetValuesResponse response;
  TextFormat::ParseFromString(
      R"pb(
        compression_groups { compression_group_id: 1 content: "abc" }
        compression_groups { compression_group_id: 2 ttl_ms: 2 content: "abcd" }
      )pb",
      &response);
  absl::StatusOr<std::string> cbor_encoded_proto_maybe =
      V2GetValuesResponseCborEncode(response);
  ASSERT_TRUE(cbor_encoded_proto_maybe.ok())
      << cbor_encoded_proto_maybe.status();
  EXPECT_EQ(json_etalon.dump(),
            ordered_json::from_cbor(*cbor_encoded_proto_maybe).dump());
}

TEST(CborConverterTest, V2CompressionGroupCborEncodeSuccess) {
  ordered_json json_etalon = ordered_json::parse(R"(
  {
    "partitionOutputs": [
      {
        "id": 0,
        "keyGroupOutputs": [
          {
            "tags": [
              "custom",
              "keys"
            ],
            "keyValues": {
              "hello": {
                "value": "world"
              }
            }
          },
          {
            "tags": [
              "structured",
              "groupNames"
            ],
            "keyValues": {
              "hello": {
                "value": "world"
              }
            }
          }
        ]
      },
      {
        "id": 1,
        "keyGroupOutputs": [
          {
            "tags": [
              "custom",
              "keys"
            ],
            "keyValues": {
              "hello2": {
                "value": "world2"
              }
            }
          }
        ]
      }
    ]
  }
)");

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
  EXPECT_EQ(json_etalon.dump(),
            ordered_json::from_cbor(*cbor_encoded_proto_maybe).dump());
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

TEST(CborConverterTest, V2GetValuesRequestJsonStringCborEncodeSuccess) {
  std::string json_request = R"({
  "partitions": [
      {
          "id": 0,
          "compressionGroupId": 0,
          "arguments": [
              {
                  "tags": [
                      "structured",
                      "groupNames"
                  ],
                  "data": [
                      "hello"
                  ]
              }
          ]
      }
  ]
  })";
  absl::StatusOr<std::string> cbor_encoded_request_maybe =
      V2GetValuesRequestJsonStringCborEncode(json_request);
  ASSERT_TRUE(cbor_encoded_request_maybe.ok())
      << cbor_encoded_request_maybe.status();
  EXPECT_EQ(nlohmann::json::parse(json_request),
            nlohmann::json::from_cbor(*cbor_encoded_request_maybe));
}

TEST(CborConverterTest,
     V2GetValuesRequestJsonStringCborEncodeInvalidJsonFails) {
  std::string json_request = R"({
  "partitions": [
      {
          "id": 0,
          "compressionGroupId": 0,
          "arguments": [
              {
                  "tags": [
                      "structured",
                      "groupNames"
                  ],
                  "data": [
                      "hello"
                  ]
              }
          ]
      }
  ],
  })";
  absl::StatusOr<std::string> cbor_encoded_request_maybe =
      V2GetValuesRequestJsonStringCborEncode(json_request);
  ASSERT_FALSE(cbor_encoded_request_maybe.ok())
      << cbor_encoded_request_maybe.status();
}

TEST(CborConverterTest, V2GetValuesRequestProtoToCborEncodeSuccess) {
  v2::GetValuesRequest request;
  TextFormat::ParseFromString(
      R"pb(partitions {
             id: 0
             compression_group_id: 1
             arguments { data { string_value: "hi" } }

           })pb",
      &request);
  absl::StatusOr<std::string> cbor_encoded_request_maybe =
      V2GetValuesRequestProtoToCborEncode(request);
  ASSERT_TRUE(cbor_encoded_request_maybe.ok())
      << cbor_encoded_request_maybe.status();
}

TEST(CborConverterTest, CborDecodeToNonBytesProtoSuccess) {
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
  const auto status = CborDecodeToNonBytesProto(cbor_raw, actual);
  ASSERT_TRUE(status.ok());
  EXPECT_THAT(actual, EqualsProto(expected));
}

TEST(CborConverterTest, CborDecodeToNonBytesProtoGetValuesResponseFailure) {
  json json_etalon = R"({
    "compressionGroups": [
      {
        "compressionGroupId": 1,
        "content": {"bytes":[97,98,99],"subtype":null},
        "ttlMs": 2
      }
    ]
  })"_json;
  v2::GetValuesResponse actual;
  std::vector<std::uint8_t> v = json::to_cbor(json_etalon);
  std::string cbor_raw(v.begin(), v.end());
  const auto status = CborDecodeToNonBytesProto(cbor_raw, actual);
  ASSERT_FALSE(status.ok()) << status;
}

TEST(CborConverterTest, CborDecodeToNonBytesProtoFailure) {
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
  const auto status = CborDecodeToNonBytesProto(cbor_raw, actual);
  ASSERT_FALSE(status.ok());
}

TEST(CborConverterTest, PartitionOutputsCborEncodeSuccess) {
  ordered_json json_etalon = ordered_json::parse(R"(
    [
      {
        "id": 0,
        "keyGroupOutputs": [
          {
            "tags": [
              "custom",
              "keys"
            ],
            "keyValues": {
              "hello": {
                "value": "world"
              }
            }
          }
        ]
      },
      {
        "id": 1,
        "keyGroupOutputs": [
          {
            "tags": [
              "custom",
              "keys"
            ],
            "keyValues": {
              "hello2": {
                "value": "world2"
              }
            }
          }
        ]
      }
    ]
)");

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
      PartitionOutputsCborEncode(
          *compression_group.mutable_partition_outputs());
  ASSERT_TRUE(cbor_encoded_proto_maybe.ok())
      << cbor_encoded_proto_maybe.status();
  EXPECT_EQ(json_etalon, ordered_json::from_cbor(*cbor_encoded_proto_maybe));
}

TEST(CborConverterTest, PartitionOutputsCborEncodeEmptyKeyGroupOutputsSuccess) {
  ordered_json json_etalon = nlohmann::ordered_json::parse(R"(
   [
      {
        "id": 0,
        "keyGroupOutputs": []
      }
    ]
)");

  application_pa::V2CompressionGroup compression_group;
  TextFormat::ParseFromString(R"pb(partition_outputs { id: 0 })pb",
                              &compression_group);
  absl::StatusOr<std::string> cbor_encoded_proto_maybe =
      PartitionOutputsCborEncode(
          *compression_group.mutable_partition_outputs());
  ASSERT_TRUE(cbor_encoded_proto_maybe.ok());
  ASSERT_TRUE(json_etalon ==
              ordered_json::from_cbor(*cbor_encoded_proto_maybe));
}

TEST(CborConverterTest, PartitionOutputsCborEncodeKeyValueMapOrderSuccess) {
  ordered_json json_etalon = ordered_json::parse(R"(
    [
      {
        "id": 0,
        "keyGroupOutputs": [
          {
            "tags": [
              "custom",
              "keys"
            ],
            "keyValues": {
              "a": {
                "value": "first"
              },
              "b": {
                "value": "second"
              },
              "ab": {
                "value": "third"
              }
            }
          }
        ]
      }
    ]
)");

  application_pa::V2CompressionGroup compression_group;
  TextFormat::ParseFromString(
      R"pb(partition_outputs {
             id: 0
             key_group_outputs {
               tags: "custom"
               tags: "keys"
               key_values {
                 key: "b"
                 value { value { string_value: "second" } }
               }
               key_values {
                 key: "a"
                 value { value { string_value: "first" } }
               }
               key_values {
                 key: "ab"
                 value { value { string_value: "third" } }
               }
             }
           })pb",
      &compression_group);
  absl::StatusOr<std::string> cbor_encoded_proto_maybe =
      PartitionOutputsCborEncode(
          *compression_group.mutable_partition_outputs());
  ASSERT_TRUE(cbor_encoded_proto_maybe.ok())
      << cbor_encoded_proto_maybe.status();
  EXPECT_EQ(json_etalon, ordered_json::from_cbor(*cbor_encoded_proto_maybe));
}

}  // namespace
}  // namespace kv_server
