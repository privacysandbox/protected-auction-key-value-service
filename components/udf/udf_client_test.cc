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
#include "components/udf/hooks/get_values_hook.h"
#include "components/udf/hooks/run_query_hook.h"
#include "components/udf/mocks.h"
#include "components/udf/udf_config_builder.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "roma/config/src/config.h"
#include "roma/interface/roma.h"

using google::protobuf::TextFormat;
using google::scp::roma::Config;
using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::FunctionBindingPayload;
using google::scp::roma::WasmDataType;
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

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = () => 'Hello world!';",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
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

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = () => 'Hello world!';",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
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

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (input) => 'Hello world! ' + JSON.stringify(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({R"("ECHO")"});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! \"ECHO\"")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsEchoCallSucceeds_SimpleUDFArg_string) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (metadata, input) => 'Hello world! ' + "
            "JSON.stringify(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());

  google::protobuf::RepeatedPtrField<UDFArgument> args;
  args.Add([] {
    UDFArgument arg;
    arg.mutable_data()->set_string_value("ECHO");
    return arg;
  }());
  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({}, args);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! \"ECHO\"")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsEchoCallSucceeds_SimpleUDFArg_string_tagged) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (metadata, input) => 'Hello world! ' + "
            "JSON.stringify(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());

  google::protobuf::RepeatedPtrField<UDFArgument> args;
  args.Add([] {
    UDFArgument arg;
    arg.mutable_tags()->add_values()->set_string_value("tag1");
    arg.mutable_data()->set_string_value("ECHO");
    return arg;
  }());
  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({}, args);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result,
            R"("Hello world! {\"tags\":[\"tag1\"],\"data\":\"ECHO\"}")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}
TEST(UdfClientTest, JsEchoCallSucceeds_SimpleUDFArg_string_tagged_list) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (metadata, input) => 'Hello world! ' + "
            "JSON.stringify(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());

  google::protobuf::RepeatedPtrField<UDFArgument> args;
  args.Add([] {
    UDFArgument arg;
    arg.mutable_tags()->add_values()->set_string_value("tag1");
    auto* list_value = arg.mutable_data()->mutable_list_value();
    list_value->add_values()->set_string_value("key1");
    list_value->add_values()->set_string_value("key2");
    return arg;
  }());
  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({}, args);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(
      *result,
      R"("Hello world! {\"tags\":[\"tag1\"],\"data\":[\"key1\",\"key2\"]}")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsEchoCallSucceeds_SimpleUDFArg_struct) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (metadata, input) => 'Hello world! ' + "
            "JSON.stringify(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());

  google::protobuf::RepeatedPtrField<UDFArgument> args;
  args.Add([] {
    UDFArgument arg;
    (*arg.mutable_data()->mutable_struct_value()->mutable_fields())["key"]
        .set_string_value("value");
    return arg;
  }());
  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({}, args);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! {\"key\":\"value\"}")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

static void udfCbEcho(FunctionBindingPayload<>& payload) {
  payload.io_proto.set_output_string("Echo: " +
                                     payload.io_proto.input_string());
}

TEST(UdfClientTest, JsEchoHookCallSucceeds) {
  auto function_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_object->function_name = "echo";
  function_object->function = udfCbEcho;

  Config config;
  config.number_of_workers = 1;
  config.RegisterFunctionBinding(std::move(function_object));

  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(config);
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (input) => 'Hello world! ' + echo(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({R"("I'm a key")"});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! Echo: I'm a key")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsStringInWithGetValuesHookSucceeds) {
  auto mock_lookup = std::make_unique<MockLookup>();

  InternalLookupResponse response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   })pb",
                              &response);
  ON_CALL(*mock_lookup, GetKeyValues(_)).WillByDefault(Return(response));

  auto get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kString);
  get_values_hook->FinishInit(std::move(mock_lookup));
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      config_builder.RegisterStringGetValuesHook(*get_values_hook)
          .SetNumberOfWorkers(1)
          .Config());
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
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
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({R"("key1")"});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Key: key1, Value: value1")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsJSONObjectInWithGetValuesHookSucceeds) {
  auto mock_lookup = std::make_unique<MockLookup>();

  InternalLookupResponse response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   })pb",
                              &response);
  ON_CALL(*mock_lookup, GetKeyValues(_)).WillByDefault(Return(response));

  auto get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kString);
  get_values_hook->FinishInit(std::move(mock_lookup));
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      config_builder.RegisterStringGetValuesHook(*get_values_hook)
          .SetNumberOfWorkers(1)
          .Config());
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          const keys = input.keys;
          const kvPairs = JSON.parse(getValues(keys)).kvPairs;
          let output = "";
          for (const key in kvPairs) {
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
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({R"({"keys":["key1"]})"});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Key: key1, Value: value1")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsJSONObjectInWithRunQueryHookSucceeds) {
  auto mock_lookup = std::make_unique<MockLookup>();

  InternalRunQueryResponse response;
  TextFormat::ParseFromString(R"pb(elements: "a")pb", &response);
  ON_CALL(*mock_lookup, RunQuery(_)).WillByDefault(Return(response));

  auto run_query_hook = RunQueryHook::Create();
  run_query_hook->FinishInit(std::move(mock_lookup));
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(config_builder.RegisterRunQueryHook(*run_query_hook)
                            .SetNumberOfWorkers(1)
                            .Config());
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          let keys = input.keys;
          let queryResultArray = runQuery(keys[0]);
          return queryResultArray;
        }
      )",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({R"({"keys":["key1"]})"});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"(["a"])");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, JsCallsLogMessageTwiceSucceeds) {
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      config_builder.RegisterLoggingHook().SetNumberOfWorkers(1).Config());
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          const a = logMessage("first message");
          const b = logMessage("second message");
          return a + b;
        }
      )",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());

  absl::StatusOr<std::string> result =
      udf_client.value()->ExecuteCode({R"({"keys":["key1"]})"});
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST(UdfClientTest, UpdatesCodeObjectTwice) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  auto status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello1 = () => '1';",
      .udf_handler_name = "hello1",
      .logical_commit_time = 1,
      .version = 1,
  });

  EXPECT_TRUE(status.ok());
  status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello2 = () => '2';",
      .udf_handler_name = "hello2",
      .logical_commit_time = 2,
      .version = 2,
  });
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

  auto status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello1 = () => '1';",
      .udf_handler_name = "hello1",
      .logical_commit_time = 1,
      .version = 1,
  });

  EXPECT_TRUE(status.ok());
  status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello2 = () => '2';",
      .udf_handler_name = "hello2",
      .logical_commit_time = 1,
      .version = 1,
  });
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

  auto status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello1 = () => '1';",
      .udf_handler_name = "hello1",
      .logical_commit_time = 2,
      .version = 1,
  });

  EXPECT_TRUE(status.ok());
  status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello2 = () => '2';",
      .udf_handler_name = "hello2",
      .logical_commit_time = 1,
      .version = 1,
  });
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
