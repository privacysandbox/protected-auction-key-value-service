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

#include "public/query/cpp/client_utils.h"

#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;

TEST(ClientUtils, ToJson) {
  v2::GetValuesRequest req;
  TextFormat::ParseFromString(
      R"pb(
        client_version: "Example.20231018"
        partitions {
          id: 0
          arguments { data { string_value: "a1" } }
          arguments {
            data {
              struct_value {
                fields {
                  key: "m1"
                  value { string_value: "v1" }
                }
                fields {
                  key: "m2"
                  value { string_value: "v2" }
                }
                fields {
                  key: "m3"
                  value { string_value: "v3" }
                }
              }
            }
          }
          arguments { data { string_value: "a3" } }
        })pb",
      &req);
  auto maybe_json = ToJson(req);
  ASSERT_TRUE(maybe_json.ok());
  EXPECT_EQ(
      *maybe_json,
      R"({"clientVersion":"Example.20231018","partitions":[{"arguments":[{"data":"a1"},{"data":{"m1":"v1","m2":"v2","m3":"v3"}},{"data":"a3"}]}]})");
}
}  // namespace
}  // namespace kv_server
