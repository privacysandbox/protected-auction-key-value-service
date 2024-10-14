/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "public/applications/pa/response_utils.h"

#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "public/test_util/proto_matcher.h"

namespace kv_server::application_pa {
namespace {

using google::protobuf::TextFormat;

TEST(ResponseUtils, PartitionOutputFromAndToJson) {
  PartitionOutput proto;
  TextFormat::ParseFromString(
      R"(
        key_group_outputs {
          tags: "tag1"
          tags: "tag2"
          key_values: {
            key: "key1"
            value: {
              value: {
                string_value: "str_val"
              }
            }
          }
          key_values: {
            key: "key2"
            value: {
              value: {
                list_value: {
                  values: {
                    string_value: "item1"
                  }
                  values: {
                    string_value: "item2"
                  }
                  values: {
                    string_value: "item3"
                  }
                }
              }
            }
          }
        }
      )",
      &proto);
  auto maybe_json = PartitionOutputToJson(proto);
  ASSERT_TRUE(maybe_json.ok());
  std::string expected_json =
      "{\"keyGroupOutputs\":[{\"tags\":[\"tag1\",\"tag2\"],\"keyValues\":{"
      "\"key1\":{\"value\":\"str_val\"},\"key2\":{\"value\":[\"item1\","
      "\"item2\",\"item3\"]}}}]}";
  EXPECT_EQ(*maybe_json, expected_json);
  auto maybe_proto = PartitionOutputFromJson(expected_json);
  ASSERT_TRUE(maybe_proto.ok());
  EXPECT_THAT(*maybe_proto, EqualsProto(proto));
}

TEST(ResponseUtils, PartitionOutputFromJson_InvalidJsonError) {
  std::string expected_json =
      "{\"keyGroupOutputs\":{\"tags\":[\"tag1\",\"tag2\"],\"keyValues\":{"
      "\"key1\":{\"value\":\"str_val\"},\"key2\":{\"value\":[\"item1\","
      "\"item2\",\"item3\"]}}}]}";
  const auto maybe_proto = PartitionOutputFromJson(expected_json);
  ASSERT_FALSE(maybe_proto.ok());
}

}  // namespace
}  // namespace kv_server::application_pa
