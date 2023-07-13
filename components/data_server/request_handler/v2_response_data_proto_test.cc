// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "components/data_server/request_handler/v2_response_data.pb.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/json_util.h"
#include "gtest/gtest.h"
#include "public/test_util/proto_matcher.h"

using google::protobuf::TextFormat;
using testing::_;
using testing::Return;

using google::protobuf::util::JsonStringToMessage;
using google::protobuf::util::MessageToJsonString;

namespace kv_server {
namespace {

TEST(V2CompressionGroupProtoTest,
     SuccessfullyParsesV2ResponseCompressionGroup) {
  V2CompressionGroup v2_response_data_proto;
  std::string v2_response_data_json = R"(
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
)";
  auto json_to_proto_status =
      JsonStringToMessage(v2_response_data_json, &v2_response_data_proto);
  EXPECT_TRUE(json_to_proto_status.ok());
  EXPECT_EQ(json_to_proto_status.message().as_string(), "");
  V2CompressionGroup expected;
  TextFormat::ParseFromString(
      R"pb(partitions {
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
      &expected);
  EXPECT_THAT(v2_response_data_proto, EqualsProto(expected));
}

}  // namespace
}  // namespace kv_server
