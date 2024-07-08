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
#include "google/protobuf/message_lite.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "public/test_util/proto_matcher.h"
#include "public/udf/binary_get_values.pb.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using google::scp::roma::FunctionBindingPayload;
using google::scp::roma::proto::FunctionBindingIoProto;
using testing::_;
using testing::Return;

class GetValuesHookTest : public ::testing::Test {
 protected:
  GetValuesHookTest() {
    InitMetricsContextMap();
    request_context_ = std::make_shared<RequestContext>();
  }
  std::shared_ptr<RequestContext> GetRequestContext() {
    return request_context_;
  }
  std::shared_ptr<RequestContext> request_context_;
};

TEST_F(GetValuesHookTest, StringOutput_SuccessfullyProcessesValue) {
  absl::flat_hash_set<std::string_view> keys = {"key1", "key2"};
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
  auto mock_lookup = std::make_unique<MockLookup>();
  EXPECT_CALL(*mock_lookup, GetKeyValues(_, keys))
      .WillOnce(Return(lookup_response));

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(
      R"pb(input_list_of_string { data: "key1" data: "key2" })pb", &io);
  auto get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kString);
  get_values_hook->FinishInit(std::move(mock_lookup));
  FunctionBindingPayload<std::weak_ptr<RequestContext>> payload{
      io, GetRequestContext()};
  (*get_values_hook)(payload);

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
  EXPECT_EQ(result_json["status"]["code"], 0);
  EXPECT_EQ(result_json["status"]["message"], "ok");
}

TEST_F(GetValuesHookTest, StringOutput_SuccessfullyProcessesResultsWithStatus) {
  absl::flat_hash_set<std::string_view> keys = {"key1"};
  InternalLookupResponse lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { status { code: 2, message: "Some error" } }
           })pb",
      &lookup_response);

  auto mock_lookup = std::make_unique<MockLookup>();
  EXPECT_CALL(*mock_lookup, GetKeyValues(_, keys))
      .WillOnce(Return(lookup_response));

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_list_of_string { data: "key1" })pb",
                              &io);
  auto get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kString);
  get_values_hook->FinishInit(std::move(mock_lookup));
  FunctionBindingPayload<std::weak_ptr<RequestContext>> payload{
      io, GetRequestContext()};
  (*get_values_hook)(payload);

  nlohmann::json expected =
      R"({"kvPairs":{"key1":{"status":{"code":2,"message":"Some error"}}},"status":{"code":0,"message":"ok"}})"_json;
  EXPECT_EQ(io.output_string(), expected.dump());
}

TEST_F(GetValuesHookTest, StringOutput_LookupReturnsError) {
  absl::flat_hash_set<std::string_view> keys = {"key1"};
  auto mock_lookup = std::make_unique<MockLookup>();
  EXPECT_CALL(*mock_lookup, GetKeyValues(_, keys))
      .WillOnce(Return(absl::UnknownError("Some error")));

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_list_of_string { data: "key1" })pb",
                              &io);
  auto get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kString);
  get_values_hook->FinishInit(std::move(mock_lookup));
  FunctionBindingPayload<std::weak_ptr<RequestContext>> payload{
      io, GetRequestContext()};
  (*get_values_hook)(payload);

  nlohmann::json expected = R"({"code":2,"message":"Some error"})"_json;
  EXPECT_EQ(io.output_string(), expected.dump());
}

TEST_F(GetValuesHookTest, StringOutput_InputIsNotListOfStrings) {
  absl::flat_hash_set<std::string_view> keys = {"key1"};
  auto mock_lookup = std::make_unique<MockLookup>();

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_string: "key1")pb", &io);
  auto get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kString);
  get_values_hook->FinishInit(std::move(mock_lookup));
  FunctionBindingPayload<std::weak_ptr<RequestContext>> payload{
      io, GetRequestContext()};
  (*get_values_hook)(payload);

  nlohmann::json expected =
      R"({"code":3,"message":"getValues input must be list of strings"})"_json;
  EXPECT_EQ(io.output_string(), expected.dump());
}

TEST_F(GetValuesHookTest, BinaryOutput_SuccessfullyProcessesValue) {
  absl::flat_hash_set<std::string_view> keys = {"key1", "key2"};
  InternalLookupResponse lookup_response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { value: "value1" }
           }
           kv_pairs {
             key: "key2"
             value { status { code: 2, message: "Some error" } }
           })pb",
      &lookup_response);
  auto mock_lookup = std::make_unique<MockLookup>();
  EXPECT_CALL(*mock_lookup, GetKeyValues(_, keys))
      .WillOnce(Return(lookup_response));

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(
      R"pb(input_list_of_string { data: "key1" data: "key2" })pb", &io);
  auto get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kBinary);
  get_values_hook->FinishInit(std::move(mock_lookup));
  FunctionBindingPayload<std::weak_ptr<RequestContext>> payload{
      io, GetRequestContext()};
  (*get_values_hook)(payload);

  EXPECT_TRUE(io.has_output_bytes());
  std::string data = io.output_bytes();

  BinaryGetValuesResponse response;
  response.ParseFromArray(data.data(), data.size());
  EXPECT_EQ(response.status().code(), 0);
  EXPECT_EQ(response.status().message(), "ok");

  Value value_with_data;
  TextFormat::ParseFromString(R"pb(data: "value1")pb", &value_with_data);
  EXPECT_TRUE(response.kv_pairs().contains("key1"));
  EXPECT_THAT(response.kv_pairs().at("key1"), EqualsProto(value_with_data));

  Value value_with_status;
  TextFormat::ParseFromString(
      R"pb(status { code: 2, message: "Some error" })pb", &value_with_status);
  EXPECT_TRUE(response.kv_pairs().contains("key2"));
  EXPECT_THAT(response.kv_pairs().at("key2"), EqualsProto(value_with_status));
}

TEST_F(GetValuesHookTest, BinaryOutput_LookupReturnsError) {
  absl::flat_hash_set<std::string_view> keys = {"key1"};
  auto mock_lookup = std::make_unique<MockLookup>();
  EXPECT_CALL(*mock_lookup, GetKeyValues(_, keys))
      .WillOnce(Return(absl::UnknownError("Some error")));

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_list_of_string { data: "key1" })pb",
                              &io);
  auto get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kBinary);
  get_values_hook->FinishInit(std::move(mock_lookup));
  FunctionBindingPayload<std::weak_ptr<RequestContext>> payload{
      io, GetRequestContext()};
  (*get_values_hook)(payload);

  EXPECT_TRUE(io.has_output_bytes());
  std::string data = io.output_bytes();

  BinaryGetValuesResponse response;
  response.ParseFromArray(data.data(), data.size());
  EXPECT_EQ(response.status().code(), 2);
  EXPECT_EQ(response.status().message(), "Some error");
}

}  // namespace
}  // namespace kv_server
