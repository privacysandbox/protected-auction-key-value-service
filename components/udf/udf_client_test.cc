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
#include "components/internal_lookup/mocks.h"
#include "components/udf/code_config.h"
#include "components/udf/get_values_hook_impl.h"
#include "components/udf/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "roma/config/src/config.h"
#include "roma/config/src/function_binding_object.h"
#include "roma/interface/roma.h"

using google::protobuf::TextFormat;
using google::scp::roma::Config;
using google::scp::roma::FunctionBindingObject;
using google::scp::roma::WasmDataType;
using testing::_;
using testing::Return;

namespace kv_server {
namespace {

absl::StatusOr<std::unique_ptr<UdfClient>> CreateUdfClient() {
  Config config;
  config.NumberOfWorkers = 1;
  return UdfClient::Create(config);
}

TEST(UdfClientTest, JsCallSucceeds) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(
      CodeConfig{.js = R"(
    function hello() { return "Hello world!"; }
  )",
                 .udf_handler_name = "hello"});
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

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(
      CodeConfig{.js = R"(
    function hello() { return "Hello world!"; }
  )",
                 .udf_handler_name = "hello"});
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

TEST(UdfClientTest, WasmCallSucceeds) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  CodeConfig code_config;
  // https://github.com/v8/v8/blob/5fe0aa3bc79c0a9d3ad546b79211f07105f09585/samples/hello-world.cc#L66
  char wasm_bin[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01,
                     0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03,
                     0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x61, 0x64,
                     0x64, 0x00, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20,
                     0x00, 0x20, 0x01, 0x6a, 0x0b};
  code_config.wasm.assign(wasm_bin, sizeof(wasm_bin));
  code_config.udf_handler_name = "add";

  absl::Status code_obj_status = udf_client.value()->SetWasmCodeObject(
      std::move(code_config),
      /*wasm_return_type=*/WasmDataType::kUint32);
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({"1", "2"});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, "3");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, WasmFromFileSucceeds) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  std::ifstream ifs("components/test_data/add.wasm");
  std::string content((std::istreambuf_iterator<char>(ifs)),
                      (std::istreambuf_iterator<char>()));

  CodeConfig code_config;
  code_config.wasm = content;
  code_config.udf_handler_name = "add";
  absl::Status code_obj_status = udf_client.value()->SetWasmCodeObject(
      std::move(code_config),
      /*wasm_return_type=*/WasmDataType::kUint32);
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({"1", "2"});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, "3");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsEchoCallSucceeds) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(
      CodeConfig{.js = R"(
    function hello(input) { return "Hello world! " + JSON.stringify(input); }
  )",
                 .udf_handler_name = "hello"});
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({"\"ECHO\""});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! \"ECHO\"")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

static std::string Echo(std::tuple<std::string>& input) {
  return "Echo: " + std::get<0>(input);
}

TEST(UdfClientTest, JsEchoHookCallSucceeds) {
  auto function_object =
      std::make_unique<FunctionBindingObject<std::string, std::string>>();
  function_object->function_name = "echo";
  function_object->function = Echo;

  Config config;
  config.NumberOfWorkers = 1;
  config.RegisterFunctionBinding(std::move(function_object));

  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(config);
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(
      CodeConfig{.js = R"(
function hello(input) { return "Hello world! " + echo(input); }
  )",
                 .udf_handler_name = "hello"});
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({"\"I'm a key\""});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! Echo: I'm a key")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsStringInWithGetValuesHookSucceeds) {
  MockLookupClient mock_lookup_client;

  InternalLookupResponse response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   })pb",
                              &response);
  ON_CALL(mock_lookup_client, GetValues(_)).WillByDefault(Return(response));

  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(UdfClient::ConfigWithGetValuesHook(
          *NewGetValuesHook(
              [&]() -> LookupClient& { return mock_lookup_client; }),
          1));
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(
      CodeConfig{.js = R"(
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
                 .udf_handler_name = "hello"});
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({R"("key1")"});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Key: key1, Value: value1")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsJSONObjectInWithGetValuesHookSucceeds) {
  MockLookupClient mock_lookup_client;

  InternalLookupResponse response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   })pb",
                              &response);
  ON_CALL(mock_lookup_client, GetValues(_)).WillByDefault(Return(response));

  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(UdfClient::ConfigWithGetValuesHook(
          *NewGetValuesHook(
              [&]() -> LookupClient& { return mock_lookup_client; }),
          1));
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(
      CodeConfig{.js = R"(
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
                 .udf_handler_name = "hello"});
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({R"({"keys":["key1"]})"});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Key: key1, Value: value1")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, UpdatesCodeObjectTwice) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  auto status = udf_client.value()->SetCodeObject(
      CodeConfig{.js = R"(
    function hello1() { return "1"; }
  )",
                 .udf_handler_name = "hello1"});

  EXPECT_TRUE(status.ok());
  status = udf_client.value()->SetCodeObject(
      CodeConfig{.js = R"(
    function hello2() { return "2"; }
  )",

                 .udf_handler_name = "hello2"});
  EXPECT_TRUE(status.ok());

  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode({});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("2")");

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
