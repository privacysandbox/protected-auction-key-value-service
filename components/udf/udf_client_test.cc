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

#include "components/udf/udf_client.h"

#include <fstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "components/internal_server/mocks.h"
#include "components/udf/code_config.h"
#include "components/udf/get_values_hook.h"
#include "components/udf/mocks.h"
#include "components/udf/run_query_hook.h"
#include "components/udf/udf_config_builder.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "roma/config/src/config.h"
#include "roma/config/src/function_binding_object.h"
#include "roma/interface/roma.h"

using google::protobuf::TextFormat;
using google::scp::roma::Config;
using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::WasmDataType;
using google::scp::roma::proto::FunctionBindingIoProto;
using testing::_;
using testing::Return;

namespace kv_server {
namespace {

absl::StatusOr<std::unique_ptr<UdfClient>> CreateUdfClient() {
  Config config;
  config.number_of_workers = 1;
  return UdfClient::Create(config);
}

TEST(UdfClientTest, UdfClient_Create_Success) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());
  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsCallSucceeds) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status =
      udf_client.value()->SetCodeObject(CodeConfig{.js = R"(
    function hello() { return "Hello world!"; }
  )",
                                                   .udf_handler_name = "hello",
                                                   .logical_commit_time = 1,
                                                   .version = 1});
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode({});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world!")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, RepeatedJsCallsSucceed) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status =
      udf_client.value()->SetCodeObject(CodeConfig{.js = R"(
    function hello() { return "Hello world!"; }
  )",
                                                   .udf_handler_name = "hello",
                                                   .logical_commit_time = 1,
                                                   .version = 1});
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result1 = udf_client.value()->ExecuteCode({});
  EXPECT_TRUE(result1.ok());
  EXPECT_EQ(*result1, R"("Hello world!")");

  absl::StatusOr<std::string> result2 = udf_client.value()->ExecuteCode({});
  EXPECT_TRUE(result2.ok());
  EXPECT_EQ(*result2, R"("Hello world!")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsEchoCallSucceeds) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status =
      udf_client.value()->SetCodeObject(CodeConfig{.js = R"(
  function hello(input) { return "Hello world! " + JSON.stringify(input); }
  )",
                                                   .udf_handler_name = "hello",
                                                   .logical_commit_time = 1,
                                                   .version = 1});
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({"\"ECHO\""});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! \"ECHO\"")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

static void Echo(FunctionBindingIoProto& io) {
  io.set_output_string("Echo: " + io.input_string());
}

TEST(UdfClientTest, JsEchoHookCallSucceeds) {
  auto function_object = std::make_unique<FunctionBindingObjectV2>();
  function_object->function_name = "echo";
  function_object->function = Echo;

  Config config;
  config.number_of_workers = 1;
  config.RegisterFunctionBinding(std::move(function_object));

  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(config);
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status =
      udf_client.value()->SetCodeObject(CodeConfig{.js = R"(
function hello(input) { return "Hello world! " + echo(input); }
  )",
                                                   .udf_handler_name = "hello",
                                                   .logical_commit_time = 1,
                                                   .version = 1});
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({"\"I'm a key\""});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! Echo: I'm a key")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsStringInWithGetValuesHookSucceeds) {
  auto mlc = std::make_unique<MockLookupClient>();
  MockLookupClient* mock_lookup_client = mlc.get();

  InternalLookupResponse response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   })pb",
                              &response);
  ON_CALL(*mock_lookup_client, GetValues(_)).WillByDefault(Return(response));

  auto get_values_hook = GetValuesHook::Create(
      [mlc = std::move(mlc)]() mutable { return std::move(mlc); });
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(config_builder.RegisterGetValuesHook(*get_values_hook)
                            .SetNumberOfWorkers(1)
                            .Config());
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status =
      udf_client.value()->SetCodeObject(CodeConfig{.js = R"(
function hello(input) {
            let kvPairs = JSON.parse(getValues([input])).kvPairs;
            let output = "";
            for(const key in kvPairs) {
              output += "Key: " + key;
              if (kvPairs[key].hasOwnProperty("value")) {
                output += ", Value: " + kvPairs[key].value;
              }
            }
            return output;
          }  )",
                                                   .udf_handler_name = "hello",
                                                   .logical_commit_time = 1,
                                                   .version = 1});
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({R"("key1")"});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Key: key1, Value: value1")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsJSONObjectInWithGetValuesHookSucceeds) {
  auto mlc = std::make_unique<MockLookupClient>();
  MockLookupClient* mock_lookup_client = mlc.get();

  InternalLookupResponse response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   })pb",
                              &response);
  ON_CALL(*mock_lookup_client, GetValues(_)).WillByDefault(Return(response));

  auto get_values_hook = GetValuesHook::Create(
      [mlc = std::move(mlc)]() mutable { return std::move(mlc); });
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(config_builder.RegisterGetValuesHook(*get_values_hook)
                            .SetNumberOfWorkers(1)
                            .Config());
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status =
      udf_client.value()->SetCodeObject(CodeConfig{.js = R"(
    function hello(input) {
      let keys = input.keys;
      let kvPairs = JSON.parse(getValues(keys)).kvPairs;
      let output = "";
      for(const key in kvPairs) {
        output += "Key: " + key;
        if (kvPairs[key].hasOwnProperty("value")) {
          output += ", Value: " + kvPairs[key].value;
        }
      }
      return output;
    }
  )",
                                                   .udf_handler_name = "hello",
                                                   .logical_commit_time = 1,
                                                   .version = 1});
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({R"({"keys":["key1"]})"});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Key: key1, Value: value1")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsJSONObjectInWithRunQueryHookSucceeds) {
  auto mrq = std::make_unique<MockRunQueryClient>();
  MockRunQueryClient* mock_run_query_client = mrq.get();

  InternalRunQueryResponse response;
  TextFormat::ParseFromString(R"pb(elements: "a")pb", &response);
  ON_CALL(*mock_run_query_client, RunQuery(_)).WillByDefault(Return(response));

  auto run_query_hook = RunQueryHook::Create(
      [mrq = std::move(mrq)]() mutable { return std::move(mrq); });
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(config_builder.RegisterRunQueryHook(*run_query_hook)
                            .SetNumberOfWorkers(1)
                            .Config());
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status =
      udf_client.value()->SetCodeObject(CodeConfig{.js = R"(
    function hello(input) {
      let keys = input.keys;
      let queryResultArray = runQuery(keys[0]);
      return queryResultArray;
    }
  )",
                                                   .udf_handler_name = "hello",
                                                   .logical_commit_time = 1,
                                                   .version = 1});
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({R"({"keys":["key1"]})"});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"(["a"])");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, UpdatesCodeObjectTwice) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  auto status =
      udf_client.value()->SetCodeObject(CodeConfig{.js = R"(
    function hello1() { return "1"; }
  )",
                                                   .udf_handler_name = "hello1",
                                                   .logical_commit_time = 1,
                                                   .version = 1});

  EXPECT_TRUE(status.ok());
  status =
      udf_client.value()->SetCodeObject(CodeConfig{.js = R"(
    function hello2() { return "2"; }
  )",

                                                   .udf_handler_name = "hello2",
                                                   .logical_commit_time = 2,
                                                   .version = 2});
  EXPECT_TRUE(status.ok());

  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode({});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("2")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, IgnoresCodeObjectWithSameCommitTime) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  auto status =
      udf_client.value()->SetCodeObject(CodeConfig{.js = R"(
    function hello1() { return "1"; }
  )",
                                                   .udf_handler_name = "hello1",
                                                   .logical_commit_time = 1,
                                                   .version = 1});

  EXPECT_TRUE(status.ok());
  status =
      udf_client.value()->SetCodeObject(CodeConfig{.js = R"(
    function hello2() { return "2"; }
  )",

                                                   .udf_handler_name = "hello2",
                                                   .logical_commit_time = 1,
                                                   .version = 1});
  EXPECT_TRUE(status.ok());

  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode({});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("1")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, IgnoresCodeObjectWithSmallerCommitTime) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  auto status =
      udf_client.value()->SetCodeObject(CodeConfig{.js = R"(
    function hello1() { return "1"; }
  )",
                                                   .udf_handler_name = "hello1",
                                                   .logical_commit_time = 2,
                                                   .version = 1});

  EXPECT_TRUE(status.ok());
  status =
      udf_client.value()->SetCodeObject(CodeConfig{.js = R"(
    function hello2() { return "2"; }
  )",
                                                   .udf_handler_name = "hello2",
                                                   .logical_commit_time = 1,
                                                   .version = 1});
  EXPECT_TRUE(status.ok());

  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode({});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("1")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, CodeObjectNotSetError) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode({});
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

}  // namespace

}  // namespace kv_server
