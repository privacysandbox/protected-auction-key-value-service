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
#include "components/udf/hooks/get_values_hook.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/internal_server/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "public/test_util/proto_matcher.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using google::scp::roma::proto::FunctionBindingIoProto;
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
  auto mlc = std::make_unique<MockLookupClient>();
  MockLookupClient* mock_lookup_client = mlc.get();
  EXPECT_CALL(*mock_lookup_client, GetValues(keys))
      .WillOnce(Return(lookup_response));

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(
      R"pb(input_list_of_string { data: "key1" data: "key2" })pb", &io);
  auto get_values_hook = GetValuesHook::Create(
      [mlc = std::move(mlc)]() mutable { return std::move(mlc); },
      GetValuesHook::OutputType::kString);
  (*get_values_hook)(io);

  nlohmann::json result_json =
      nlohmann::json::parse(io.output_string(), nullptr,
                            /*allow_exceptions=*/false,
                            /*ignore_comments=*/true);
  EXPECT_FALSE(result_json.is_discarded());
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

  auto mlc = std::make_unique<MockLookupClient>();
  MockLookupClient* mock_lookup_client = mlc.get();
  EXPECT_CALL(*mock_lookup_client, GetValues(keys))
      .WillOnce(Return(lookup_response));

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_list_of_string { data: "key1" })pb",
                              &io);
  auto get_values_hook = GetValuesHook::Create(
      [mlc = std::move(mlc)]() mutable { return std::move(mlc); },
      GetValuesHook::OutputType::kString);
  (*get_values_hook)(io);

  nlohmann::json expected =
      R"({"kvPairs":{"key1":{"status":{"code":2,"message":"Some error"}}}})"_json;
  EXPECT_EQ(io.output_string(), expected.dump());
}

TEST(GetValuesHookTest, LookupClientReturnsError) {
  std::vector<std::string> keys = {"key1"};
  auto mlc = std::make_unique<MockLookupClient>();
  MockLookupClient* mock_lookup_client = mlc.get();
  EXPECT_CALL(*mock_lookup_client, GetValues(keys))
      .WillOnce(Return(absl::UnknownError("Some error")));

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_list_of_string { data: "key1" })pb",
                              &io);
  auto get_values_hook = GetValuesHook::Create(
      [mlc = std::move(mlc)]() mutable { return std::move(mlc); },
      GetValuesHook::OutputType::kString);
  (*get_values_hook)(io);

  nlohmann::json expected = R"({"code":2,"message":"Some error"})"_json;
  EXPECT_EQ(io.output_string(), expected.dump());
}

TEST(GetValuesHookTest, InputIsNotListOfStrings) {
  std::vector<std::string> keys = {"key1"};
  auto mlc = std::make_unique<MockLookupClient>();

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_string: "key1")pb", &io);
  auto get_values_hook = GetValuesHook::Create(
      [mlc = std::move(mlc)]() mutable { return std::move(mlc); },
      GetValuesHook::OutputType::kString);
  (*get_values_hook)(io);

  nlohmann::json expected =
      R"({"code":3,"message":"getValues input must be list of strings"})"_json;
  EXPECT_EQ(io.output_string(), expected.dump());
}

}  // namespace
}  // namespace kv_server
