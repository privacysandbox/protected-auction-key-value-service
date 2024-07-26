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

}  // namespace
}  // namespace kv_server
