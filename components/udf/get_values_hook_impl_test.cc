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
#include "components/udf/get_values_hook_impl.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/internal_lookup/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using testing::_;
using testing::Return;

TEST(GetValuesHookTest, SuccessfullyProcessesValue) {
  std::vector<std::string> keys = {"key1", "key2"};
  InternalLookupResponse lookup_response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   }
                                   kv_pairs {
                                     key: "key2"
                                     value { value: "value2" }
                                   })pb",
                              &lookup_response);
  MockLookupClient mock_lookup_client;
  EXPECT_CALL(mock_lookup_client, GetValues(keys))
      .WillOnce(Return(lookup_response));

  auto input = std::make_tuple(keys);
  auto get_values_hook =
      NewGetValuesHook([&]() -> LookupClient& { return mock_lookup_client; });

  std::string result = (*get_values_hook)(input);
  nlohmann::json result_json = nlohmann::json::parse(result);
  nlohmann::json expected_value1 = R"({"value":"value1"})"_json;
  nlohmann::json expected_value2 = R"({"value":"value2"})"_json;
  EXPECT_TRUE(result_json.contains("kvPairs"));
  EXPECT_EQ(result_json["kvPairs"]["key1"], expected_value1);
  EXPECT_EQ(result_json["kvPairs"]["key2"], expected_value2);
}

TEST(GetValuesHookTest, SuccessfullyProcessesResultsWithStatus) {
  std::vector<std::string> keys = {"key1"};
  InternalLookupResponse lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { status { code: 2, message: "Some error" } }
           })pb",
      &lookup_response);
  MockLookupClient mock_lookup_client;
  EXPECT_CALL(mock_lookup_client, GetValues(keys))
      .WillOnce(Return(lookup_response));

  auto input = std::make_tuple(keys);
  auto get_values_hook =
      NewGetValuesHook([&]() -> LookupClient& { return mock_lookup_client; });

  std::string result = (*get_values_hook)(input);
  nlohmann::json expected =
      R"({"kvPairs":{"key1":{"status":{"code":2,"message":"Some error"}}}})"_json;
  EXPECT_EQ(result, expected.dump());
}

TEST(GetValuesHookTest, LookupClientReturnsError) {
  std::vector<std::string> keys = {"key1"};
  MockLookupClient mock_lookup_client;
  EXPECT_CALL(mock_lookup_client, GetValues(keys))
      .WillOnce(Return(absl::UnknownError("Some error")));

  auto input = std::make_tuple(keys);
  auto get_values_hook =
      NewGetValuesHook([&]() -> LookupClient& { return mock_lookup_client; });

  std::string result = (*get_values_hook)(input);
  nlohmann::json expected = R"({"code":2,"message":"Some error"})"_json;
  EXPECT_EQ(result, expected.dump());
}

}  // namespace
}  // namespace kv_server
