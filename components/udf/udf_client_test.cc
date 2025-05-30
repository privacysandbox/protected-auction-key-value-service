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
#include "opentelemetry/exporters/ostream/log_record_exporter.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "public/query/v2/get_values_v2.pb.h"
#include "public/test_util/request_example.h"
#include "public/udf/constants.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"

using google::protobuf::TextFormat;
using google::scp::roma::Config;
using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::FunctionBindingPayload;
using testing::_;
using testing::ContainsRegex;
using testing::HasSubstr;
using testing::Return;

namespace kv_server {
namespace {

constexpr std::string_view kEmptyMetadata = R"(
request_metadata {
  fields {
    key: "hostname"
    value {
      string_value: ""
    }
  }
}
  )";

std::string GetWasmByteString() {
  // Taken from:
  // https://github.com/v8/v8/blob/5fe0aa3bc79c0a9d3ad546b79211f07105f09585/samples/hello-world.cc#L69C6-L75C12
  // Needs to be a valid wasm binary for Roma V8 to compile
  std::vector<uint8_t> wasm_bin = {
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
      0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
      0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
      0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b};
  return std::string(wasm_bin.begin(), wasm_bin.end());
}

absl::StatusOr<std::unique_ptr<UdfClient>> CreateUdfClient() {
  Config<std::weak_ptr<RequestContext>> config;
  config.number_of_workers = 1;
  return UdfClient::Create(std::move(config));
}

class UdfClientTest : public ::testing::Test {
 protected:
  UdfClientTest() {
    privacy_sandbox::server_common::log::SetGlobalPSVLogLevel(1);
    privacy_sandbox::server_common::log::ServerToken(
        kExampleConsentedDebugToken);
    const std::string telemetry_config_str = R"pb(
      mode: PROD
      custom_udf_metric {
        name: "m_1"
        description: "log metric 1"
        lower_bound: 1
        upper_bound: 10
      }
      custom_udf_metric {
        name: "m_2"
        description: "log metric 2"
        lower_bound: 1
        upper_bound: 100
      }
    )pb";
    privacy_sandbox::server_common::telemetry::TelemetryConfig config_proto;
    TextFormat::ParseFromString(telemetry_config_str, &config_proto);
    kv_server::KVServerContextMap(
        std::make_unique<
            privacy_sandbox::server_common::telemetry::BuildDependentConfig>(
            config_proto));
    kv_server::InternalLookupServerContextMap(
        std::make_unique<
            privacy_sandbox::server_common::telemetry::BuildDependentConfig>(
            config_proto));
    request_context_factory_ = std::make_unique<RequestContextFactory>(
        privacy_sandbox::server_common::LogContext(),
        privacy_sandbox::server_common::ConsentedDebugConfiguration());
    auto* logger_provider =
        opentelemetry::sdk::logs::LoggerProviderFactory::Create(
            opentelemetry::sdk::logs::SimpleLogRecordProcessorFactory::Create(
                std::make_unique<
                    opentelemetry::exporter::logs::OStreamLogRecordExporter>(
                    consented_log_output_)))
            .release();
    privacy_sandbox::server_common::log::logger_private =
        logger_provider->GetLogger("test", "").get();
  }
  std::stringstream consented_log_output_;
  std::unique_ptr<RequestContextFactory> request_context_factory_;
  ExecutionMetadata execution_metadata_;
};

TEST_F(UdfClientTest, UdfClient_Create_Success) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());
  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsCallSucceeds) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = () => 'Hello world!';",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world!")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsLoadWithWasmBinSuccess) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "var module = new WebAssembly.Module(wasm_array); hello = () => "
            "'Hello world!';",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
      .wasm_bin = GetWasmByteString(),
  });
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world!")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsLoadWithInvalidWasmBinFails) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = () => 'Hello world!';",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
      .wasm_bin = "abc",
  });
  ASSERT_FALSE(code_obj_status.ok());
  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsExceptionReturnsStatus) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "function hello() { throw new Error('Oh no!'); }",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  ASSERT_FALSE(result.ok());
  EXPECT_THAT(result.status().message(), HasSubstr("Oh no!"));

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, RepeatedJsCallsSucceed) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = () => 'Hello world!';",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result1 = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  ASSERT_TRUE(result1.ok());
  EXPECT_EQ(*result1, R"("Hello world!")");

  absl::StatusOr<std::string> result2 = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  ASSERT_TRUE(result2.ok());
  EXPECT_EQ(*result2, R"("Hello world!")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsEchoCallSucceeds) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (input) => 'Hello world! ' + JSON.stringify(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"("ECHO")"}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! \"ECHO\"")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsEchoCallSucceeds_SimpleUDFArg_string) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (metadata, input) => 'Hello world! ' + "
            "JSON.stringify(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());

  google::protobuf::RepeatedPtrField<UDFArgument> args;
  args.Add([] {
    UDFArgument arg;
    arg.mutable_data()->set_string_value("ECHO");
    return arg;
  }());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, args, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! \"ECHO\"")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsEchoCallSucceeds_SimpleUDFArg_string_tagged) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (metadata, input) => 'Hello world! ' + "
            "JSON.stringify(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());

  google::protobuf::RepeatedPtrField<UDFArgument> args;
  args.Add([] {
    UDFArgument arg;
    arg.mutable_tags()->add_values()->set_string_value("tag1");
    arg.mutable_data()->set_string_value("ECHO");
    return arg;
  }());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, args, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result,
            R"("Hello world! {\"tags\":[\"tag1\"],\"data\":\"ECHO\"}")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}
TEST_F(UdfClientTest, JsEchoCallSucceeds_SimpleUDFArg_string_tagged_list) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (metadata, input) => 'Hello world! ' + "
            "JSON.stringify(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());

  google::protobuf::RepeatedPtrField<UDFArgument> args;
  args.Add([] {
    UDFArgument arg;
    arg.mutable_tags()->add_values()->set_string_value("tag1");
    auto* list_value = arg.mutable_data()->mutable_list_value();
    list_value->add_values()->set_string_value("key1");
    list_value->add_values()->set_string_value("key2");
    return arg;
  }());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, args, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(
      *result,
      R"("Hello world! {\"tags\":[\"tag1\"],\"data\":[\"key1\",\"key2\"]}")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsEchoCallSucceeds_SimpleUDFArg_struct) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (metadata, input) => 'Hello world! ' + "
            "JSON.stringify(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());

  google::protobuf::RepeatedPtrField<UDFArgument> args;
  args.Add([] {
    UDFArgument arg;
    (*arg.mutable_data()->mutable_struct_value()->mutable_fields())["key"]
        .set_string_value("value");
    return arg;
  }());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, args, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! {\"key\":\"value\"}")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

static void udfCbEcho(
    FunctionBindingPayload<std::weak_ptr<RequestContext>>& payload) {
  payload.io_proto.set_output_string("Echo: " +
                                     payload.io_proto.input_string());
}

TEST_F(UdfClientTest, JsEchoHookCallSucceeds) {
  auto function_object = std::make_unique<
      FunctionBindingObjectV2<std::weak_ptr<RequestContext>>>();
  function_object->function_name = "echo";
  function_object->function = udfCbEcho;

  Config<std::weak_ptr<RequestContext>> config;
  config.number_of_workers = 1;
  config.RegisterFunctionBinding(std::move(function_object));
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(std::move(config));
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (input) => 'Hello world! ' + echo(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"("I'm a key")"}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! Echo: I'm a key")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsStringInWithGetValuesHookSucceeds) {
  auto mock_lookup = std::make_unique<MockLookup>();

  InternalLookupResponse response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   })pb",
                              &response);
  ON_CALL(*mock_lookup, GetKeyValues(_, _)).WillByDefault(Return(response));

  auto get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kString);
  get_values_hook->FinishInit(std::move(mock_lookup));
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      std::move(config_builder.RegisterStringGetValuesHook(*get_values_hook)
                    .SetNumberOfWorkers(1)
                    .Config()));
  ASSERT_TRUE(udf_client.ok());

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
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"("key1")"}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Key: key1, Value: value1")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsJSONObjectInWithGetValuesHookSucceeds) {
  auto mock_lookup = std::make_unique<MockLookup>();

  InternalLookupResponse response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   })pb",
                              &response);
  ON_CALL(*mock_lookup, GetKeyValues(_, _)).WillByDefault(Return(response));

  auto get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kString);
  get_values_hook->FinishInit(std::move(mock_lookup));
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      std::move(config_builder.RegisterStringGetValuesHook(*get_values_hook)
                    .SetNumberOfWorkers(1)
                    .Config()));
  ASSERT_TRUE(udf_client.ok());

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
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Key: key1, Value: value1")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsJSONObjectInWithRunQueryHookSucceeds) {
  auto mock_lookup = std::make_unique<MockLookup>();

  InternalRunQueryResponse response;
  TextFormat::ParseFromString(R"pb(elements: "a")pb", &response);
  ON_CALL(*mock_lookup, RunQuery(_, _)).WillByDefault(Return(response));

  auto run_query_hook = RunSetQueryStringHook::Create();
  run_query_hook->FinishInit(std::move(mock_lookup));
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      std::move(config_builder.RegisterRunSetQueryStringHook(*run_query_hook)
                    .SetNumberOfWorkers(1)
                    .Config()));
  ASSERT_TRUE(udf_client.ok());

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
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"(["a"])");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, VerifyJsRunSetQueryIntHookSucceeds) {
  auto mock_lookup = std::make_unique<MockLookup>();
  InternalRunSetQueryUInt32Response response;
  TextFormat::ParseFromString(R"pb(elements: 1000 elements: 1001)pb",
                              &response);
  ON_CALL(*mock_lookup, RunSetQueryUInt32(_, _))
      .WillByDefault(Return(response));
  auto run_query_hook = RunSetQueryUInt32Hook::Create();
  run_query_hook->FinishInit(std::move(mock_lookup));
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      std::move(config_builder.RegisterRunSetQueryUInt32Hook(*run_query_hook)
                    .SetNumberOfWorkers(1)
                    .Config()));
  ASSERT_TRUE(udf_client.ok());
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          let keys = input.keys;
          let bytes = runSetQueryUInt32(keys[0]);
          if (bytes instanceof Uint8Array) {
            return Array.from(new Uint32Array(bytes.buffer));
          }
          return "runSetQueryInt failed.";
        }
      )",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(*result, R"([1000,1001])");
  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsCallsLogMessageAndConsoleLogConsentedSucceeds) {
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(std::move(config_builder.RegisterLogMessageHook()
                                      .RegisterConsoleLogHook()
                                      .SetNumberOfWorkers(1)
                                      .Config()));
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          logMessage("first message");
          console.log("second message");
          console.warn("warning message");
          console.error("error message");
          return "";
        }
      )",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  privacy_sandbox::server_common::ConsentedDebugConfiguration
      consented_debug_configuration;
  consented_debug_configuration.set_is_consented(true);
  consented_debug_configuration.set_token(kExampleConsentedDebugToken);
  privacy_sandbox::server_common::LogContext log_context;
  request_context_factory_->UpdateLogContext(log_context,
                                             consented_debug_configuration);
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("")");
  auto output_log = consented_log_output_.str();
  EXPECT_THAT(output_log, ContainsRegex("first message"));
  EXPECT_THAT(output_log, ContainsRegex("second message"));
  EXPECT_THAT(output_log, ContainsRegex("warning message"));
  EXPECT_THAT(output_log, ContainsRegex("error message"));

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest,
       JsCallsLogMessagAndConsoleLogNoLogForNonConsentedRequests) {
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(std::move(config_builder.RegisterLogMessageHook()
                                      .RegisterConsoleLogHook()
                                      .SetNumberOfWorkers(1)
                                      .Config()));
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          logMessage("first message");
          console.log("second message");
          console.warn("warning message");
          console.error("error message");
          return "";
        }
      )",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  privacy_sandbox::server_common::ConsentedDebugConfiguration
      consented_debug_configuration;
  consented_debug_configuration.set_is_consented(false);
  consented_debug_configuration.set_token("mismatch_token");
  privacy_sandbox::server_common::LogContext log_context;
  request_context_factory_->UpdateLogContext(log_context,
                                             consented_debug_configuration);
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("")");
  auto output_log = consented_log_output_.str();
  EXPECT_TRUE(output_log.empty());
  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsCallsConsoleLogOnlyLogsAboveMinLogLevel) {
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      std::move(config_builder.RegisterLogMessageHook()
                    .RegisterConsoleLogHook()
                    .SetNumberOfWorkers(1)
                    .Config()),
      /*udf_timeout=*/absl::Seconds(5), /*udf_update_timeout=*/absl::Seconds(5),
      /*udf_min_log_level=*/1);
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          console.log("should not print");
          console.warn("should print warning");
          console.error("should print error");
          return "";
        }
      )",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  privacy_sandbox::server_common::ConsentedDebugConfiguration
      consented_debug_configuration;
  consented_debug_configuration.set_is_consented(true);
  consented_debug_configuration.set_token(kExampleConsentedDebugToken);
  privacy_sandbox::server_common::LogContext log_context;
  request_context_factory_->UpdateLogContext(log_context,
                                             consented_debug_configuration);
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("")");
  auto output_log = consented_log_output_.str();
  EXPECT_THAT(output_log, ContainsRegex("should print warning"));
  EXPECT_THAT(output_log, ContainsRegex("should print error"));
  EXPECT_THAT(output_log, testing::Not(ContainsRegex("should not print")));

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, UpdatesCodeObjectTwice) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  auto status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello1 = () => '1';",
      .udf_handler_name = "hello1",
      .logical_commit_time = 1,
      .version = 1,
  });

  ASSERT_TRUE(status.ok());
  status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello2 = () => '2';",
      .udf_handler_name = "hello2",
      .logical_commit_time = 2,
      .version = 2,
  });
  ASSERT_TRUE(status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("2")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, IgnoresCodeObjectWithSameCommitTime) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  auto status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello1 = () => '1';",
      .udf_handler_name = "hello1",
      .logical_commit_time = 1,
      .version = 1,
  });

  ASSERT_TRUE(status.ok());
  status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello2 = () => '2';",
      .udf_handler_name = "hello2",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("1")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, IgnoresCodeObjectWithSmallerCommitTime) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  auto status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello1 = () => '1';",
      .udf_handler_name = "hello1",
      .logical_commit_time = 2,
      .version = 1,
  });

  ASSERT_TRUE(status.ok());
  status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello2 = () => '2';",
      .udf_handler_name = "hello2",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("1")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, CodeObjectNotSetError) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, MetadataPassedSuccesfully) {
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      std::move(config_builder.SetNumberOfWorkers(1).Config()));
  ASSERT_TRUE(udf_client.ok());
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(metadata) {
          if(metadata.requestMetadata &&
              metadata.requestMetadata.is_pas)
          {
            return "true";
          }
          if(metadata.partitionMetadata &&
              metadata.partitionMetadata.partition_level_key)
          {
            return "true";
          }
          return "false";
        }
      )",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());
  v2::GetValuesRequest req;
  auto* fields = req.mutable_metadata()->mutable_fields();
  (*fields)["is_pas"].set_string_value("true");
  (*fields)["partition_level_key"].set_string_value("true");
  UDFExecutionMetadata udf_metadata;
  *udf_metadata.mutable_request_metadata() = *req.mutable_metadata();
  google::protobuf::RepeatedPtrField<UDFArgument> args;
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, std::move(udf_metadata), args,
      execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("true")");

  UDFExecutionMetadata udf_metadata_non_pas;

  result = udf_client.value()->ExecuteCode(*request_context_factory_,
                                           std::move(udf_metadata_non_pas),
                                           args, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("false")");
  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, DefaultUdfPASucceeds) {
  auto mock_lookup = std::make_unique<MockLookup>();
  InternalLookupResponse response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   })pb",
                              &response);
  ON_CALL(*mock_lookup, GetKeyValues(_, _)).WillByDefault(Return(response));

  auto get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kString);
  get_values_hook->FinishInit(std::move(mock_lookup));
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      std::move(config_builder.RegisterStringGetValuesHook(*get_values_hook)
                    .SetNumberOfWorkers(1)
                    .Config()));
  ASSERT_TRUE(udf_client.ok());
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = kDefaultUdfCodeSnippet,
      .udf_handler_name = kDefaultUdfHandlerName,
      .logical_commit_time = kDefaultLogicalCommitTime,
      .version = kDefaultVersion,
  });
  ASSERT_TRUE(code_obj_status.ok());
  UDFExecutionMetadata udf_metadata;
  google::protobuf::RepeatedPtrField<UDFArgument> args;
  args.Add([] {
    UDFArgument arg;
    TextFormat::ParseFromString(R"(
    data {
      list_value {
        values {
          string_value: "key1"
        }
      }
    })",
                                &arg);
    return arg;
  }());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, std::move(udf_metadata), args,
      execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(
      *result,
      R"({"keyGroupOutputs":[{"keyValues":{"key1":{"value":"value1"}}}],"udfOutputApiVersion":1})");
  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, DefaultUdfPasKeyLookupFails) {
  auto mock_lookup = std::make_unique<MockLookup>();
  absl::Status status = absl::InvalidArgumentError("Error!");
  ON_CALL(*mock_lookup, GetKeyValues(_, _)).WillByDefault(Return(status));
  auto get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kString);
  get_values_hook->FinishInit(std::move(mock_lookup));
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      std::move(config_builder.RegisterStringGetValuesHook(*get_values_hook)
                    .RegisterLogMessageHook()
                    .SetNumberOfWorkers(1)
                    .Config()));
  ASSERT_TRUE(udf_client.ok());
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = kDefaultUdfCodeSnippet,
      .udf_handler_name = kDefaultUdfHandlerName,
      .logical_commit_time = kDefaultLogicalCommitTime,
      .version = kDefaultVersion,
  });
  ASSERT_TRUE(code_obj_status.ok());
  v2::GetValuesRequest req;
  (*(req.mutable_metadata()->mutable_fields()))["is_pas"].set_string_value(
      "true");
  UDFExecutionMetadata udf_metadata;
  *udf_metadata.mutable_request_metadata() = *req.mutable_metadata();
  google::protobuf::RepeatedPtrField<UDFArgument> args;
  args.Add([] {
    UDFArgument arg;
    TextFormat::ParseFromString(R"(
    data {
      list_value {
        values {
          string_value: "key1"
        }
      }
    })",
                                &arg);
    return arg;
  }());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, std::move(udf_metadata), args,
      execution_metadata_);
  ASSERT_FALSE(result.ok());
  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, DefaultUdfPasSucceeds) {
  auto mock_lookup = std::make_unique<MockLookup>();
  InternalLookupResponse response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   })pb",
                              &response);
  ON_CALL(*mock_lookup, GetKeyValues(_, _)).WillByDefault(Return(response));
  auto get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kString);
  get_values_hook->FinishInit(std::move(mock_lookup));
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      std::move(config_builder.RegisterStringGetValuesHook(*get_values_hook)
                    .RegisterLogMessageHook()
                    .SetNumberOfWorkers(1)
                    .Config()));
  ASSERT_TRUE(udf_client.ok());
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = kDefaultUdfCodeSnippet,
      .udf_handler_name = kDefaultUdfHandlerName,
      .logical_commit_time = kDefaultLogicalCommitTime,
      .version = kDefaultVersion,
  });
  ASSERT_TRUE(code_obj_status.ok());
  v2::GetValuesRequest req;
  (*(req.mutable_metadata()->mutable_fields()))["is_pas"].set_string_value(
      "true");
  UDFExecutionMetadata udf_metadata;
  *udf_metadata.mutable_request_metadata() = *req.mutable_metadata();
  google::protobuf::RepeatedPtrField<UDFArgument> args;
  args.Add([] {
    UDFArgument arg;
    TextFormat::ParseFromString(R"(
    data {
      list_value {
        values {
          string_value: "key1"
        }
      }
    })",
                                &arg);
    return arg;
  }());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, std::move(udf_metadata), args,
      execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"({"key1":{"value":"value1"}})");
  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, VerifyJsRunSetQueryUInt64HookSucceeds) {
  auto mock_lookup = std::make_unique<MockLookup>();
  InternalRunSetQueryUInt64Response response;
  TextFormat::ParseFromString(R"pb(elements: 18446744073709551614
                                   elements: 18446744073709551615)pb",
                              &response);
  ON_CALL(*mock_lookup, RunSetQueryUInt64(_, _))
      .WillByDefault(Return(response));
  auto run_query_hook = RunSetQueryUInt64Hook::Create();
  run_query_hook->FinishInit(std::move(mock_lookup));
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      std::move(config_builder.RegisterRunSetQueryUInt64Hook(*run_query_hook)
                    .SetNumberOfWorkers(1)
                    .Config()));
  ASSERT_TRUE(udf_client.ok());
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          const keys = input.keys;
          const bytes = runSetQueryUInt64(keys[0]);
          if (bytes instanceof Uint8Array) {
            const uint64Array = new BigUint64Array(bytes.buffer);
            return Array.from(uint64Array, uint64 => uint64.toString());
          }
          return "runSetQueryInt failed.";
        }
      )",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(*result, "[\"18446744073709551614\",\"18446744073709551615\"]");
  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsCallsLogCustomMetricSuccess) {
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(std::move(config_builder.RegisterLogMessageHook()
                                      .RegisterCustomMetricHook()
                                      .SetNumberOfWorkers(1)
                                      .Config()));
  ASSERT_TRUE(udf_client.ok());

  // The metric name will need to match the name defined for custom metric in
  // the telemetry config proto
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          let keys = input.keys;
          let keyCount = 0;
          for (const key of keys) {
            ++keyCount;
          };
          let logMetricRequest1 = {
            name: 'm_1',
            value: keyCount,
          };
          let logMetricRequest2 = {
            name: 'm_2',
            value: 5,
          };
          let jsonBatchLogMetrics = {
            udf_metric: [logMetricRequest1, logMetricRequest2],
          };
          const batchMetricsString = JSON.stringify(jsonBatchLogMetrics);
          const output = logCustomMetric(batchMetricsString);
          logMessage(output);
          return "";
        }
      )",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  // Turn on consented logging to execute the logging callback to capture
  // custom metrics logging outcome.
  privacy_sandbox::server_common::ConsentedDebugConfiguration
      consented_debug_configuration;
  consented_debug_configuration.set_is_consented(true);
  consented_debug_configuration.set_token(kExampleConsentedDebugToken);
  privacy_sandbox::server_common::LogContext log_context;
  request_context_factory_->UpdateLogContext(log_context,
                                             consented_debug_configuration);
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("")");
  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
  auto metrics_logging_outcome = consented_log_output_.str();
  EXPECT_THAT(metrics_logging_outcome, ContainsRegex("Log metrics success"));
}

TEST_F(UdfClientTest, JsCallsLogCustomMetricJsonParseError) {
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(std::move(config_builder.RegisterLogMessageHook()
                                      .RegisterCustomMetricHook()
                                      .SetNumberOfWorkers(1)
                                      .Config()));
  ASSERT_TRUE(udf_client.ok());

  // The metric name will need to match the name defined for custom metric in
  // the telemetry config proto
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          let logMetricRequest1 = {
            name: 'm_2',
            value: 5,
          };
          let jsonBatchLogMetrics = {
            wrong_field_name: [logMetricRequest1],
          };
          const batchMetricsString = JSON.stringify(jsonBatchLogMetrics);
          const output = logCustomMetric(batchMetricsString);
          logMessage(output);
          return "";
        }
      )",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  // Turn on consented logging to execute the logging callback to capture
  // custom metrics logging outcome.
  privacy_sandbox::server_common::ConsentedDebugConfiguration
      consented_debug_configuration;
  consented_debug_configuration.set_is_consented(true);
  consented_debug_configuration.set_token(kExampleConsentedDebugToken);
  privacy_sandbox::server_common::LogContext log_context;
  request_context_factory_->UpdateLogContext(log_context,
                                             consented_debug_configuration);
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("")");
  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
  auto metrics_logging_outcome = consented_log_output_.str();
  EXPECT_THAT(metrics_logging_outcome,
              ContainsRegex("Failed to parse metrics in Json string"));
}

TEST_F(UdfClientTest, JsCallsLogCustomMetricFailedToLogError) {
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(std::move(config_builder.RegisterLogMessageHook()
                                      .RegisterCustomMetricHook()
                                      .SetNumberOfWorkers(1)
                                      .Config()));
  ASSERT_TRUE(udf_client.ok());

  // The metric name will need to match the name defined for custom metric in
  // the telemetry config proto
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          let logMetricRequest1 = {
            name: 'not_configured_metric_name',
            value: 5,
          };
          let jsonBatchLogMetrics = {
            udf_metric: [logMetricRequest1],
          };
          const batchMetricsString = JSON.stringify(jsonBatchLogMetrics);
          const output = logCustomMetric(batchMetricsString);
          logMessage(output);
          return "";
        }
      )",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  // Turn on consented logging to execute the logging callback to capture
  // custom metrics logging outcome.
  privacy_sandbox::server_common::ConsentedDebugConfiguration
      consented_debug_configuration;
  consented_debug_configuration.set_is_consented(true);
  consented_debug_configuration.set_token(kExampleConsentedDebugToken);
  privacy_sandbox::server_common::LogContext log_context;
  request_context_factory_->UpdateLogContext(log_context,
                                             consented_debug_configuration);
  ASSERT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("")");
  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
  auto metrics_logging_outcome = consented_log_output_.str();
  EXPECT_THAT(metrics_logging_outcome, ContainsRegex("Failed to log metrics"));
}

TEST_F(UdfClientTest, BatchExecuteCodeSuccess) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (metadata, data) => 'Hello world! ' + "
            "JSON.stringify(metadata) + JSON.stringify(data);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());

  UDFExecutionMetadata udf_metadata;
  TextFormat::ParseFromString(kEmptyMetadata, &udf_metadata);

  google::protobuf::RepeatedPtrField<UDFArgument> args1;
  args1.Add([] {
    UDFArgument arg;
    arg.mutable_tags()->add_values()->set_string_value("tag1");
    arg.mutable_data()->set_string_value("key1");
    return arg;
  }());
  google::protobuf::RepeatedPtrField<UDFArgument> args2;
  args2.Add([] {
    UDFArgument arg;
    arg.mutable_tags()->add_values()->set_string_value("tag2");
    arg.mutable_data()->set_string_value("key2");
    return arg;
  }());

  absl::flat_hash_map<UniquePartitionIdTuple, UDFInput> input;
  UniquePartitionIdTuple id1 = {1, 0};
  UniquePartitionIdTuple id2 = {1, 1};
  input[id1] = {.arguments = args1};
  input[id2] = {.execution_metadata = udf_metadata, .arguments = args2};
  auto result = udf_client.value()->BatchExecuteCode(
      *request_context_factory_, input, execution_metadata_);
  ASSERT_TRUE(result.ok());
  auto udf_outputs = std::move(result.value());
  EXPECT_EQ(udf_outputs.size(), 2);
  EXPECT_EQ(
      udf_outputs[id1],
      R"("Hello world! {\"udfInterfaceVersion\":1}{\"tags\":[\"tag1\"],\"data\":\"key1\"}")");
  EXPECT_EQ(
      udf_outputs[id2],
      R"("Hello world! {\"udfInterfaceVersion\":1,\"requestMetadata\":{\"hostname\":\"\"}}{\"tags\":[\"tag2\"],\"data\":\"key2\"}")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, BatchExecuteCodeIgnoresFailedPartition) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js =
          R"js(function hello(metadata, data) {
            if(data.data == "valid_key") {return 'Hello world!';}
            throw new Error('Oh no!');
          })js",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());

  UDFExecutionMetadata udf_metadata;
  TextFormat::ParseFromString(kEmptyMetadata, &udf_metadata);

  google::protobuf::RepeatedPtrField<UDFArgument> args1;
  args1.Add([] {
    UDFArgument arg;
    arg.mutable_tags()->add_values()->set_string_value("some_tag");
    arg.mutable_data()->set_string_value("valid_key");
    return arg;
  }());
  google::protobuf::RepeatedPtrField<UDFArgument> args2;
  args2.Add([] {
    UDFArgument arg;
    arg.mutable_data()->set_string_value("invalid key");
    return arg;
  }());

  absl::flat_hash_map<UniquePartitionIdTuple, UDFInput> input;
  UniquePartitionIdTuple id1 = {1, 0};
  UniquePartitionIdTuple id2 = {1, 1};

  input[id1] = {.arguments = args1};
  input[id2] = {.arguments = args2};
  auto result = udf_client.value()->BatchExecuteCode(
      *request_context_factory_, input, execution_metadata_);
  ASSERT_TRUE(result.ok());
  auto udf_outputs = std::move(result.value());
  EXPECT_EQ(udf_outputs.size(), 1);
  EXPECT_EQ(udf_outputs[id1], R"("Hello world!")");

  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, BatchExecuteCodeEmptyReturnsSuccess) {
  auto udf_client = CreateUdfClient();
  ASSERT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (metadata, data) => 'Hello world! ' + "
            "JSON.stringify(metadata) + JSON.stringify(data);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  ASSERT_TRUE(code_obj_status.ok());

  absl::flat_hash_map<UniquePartitionIdTuple, UDFInput> input;
  auto result = udf_client.value()->BatchExecuteCode(
      *request_context_factory_, input, execution_metadata_);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->size(), 0);
  absl::Status stop = udf_client.value()->Stop();
  ASSERT_TRUE(stop.ok());
}
}  // namespace
}  // namespace kv_server
