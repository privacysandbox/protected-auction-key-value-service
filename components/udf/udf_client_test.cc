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

#include "absl/log/scoped_mock_log.h"
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
absl::StatusOr<std::unique_ptr<UdfClient>> CreateUdfClient() {
  Config<std::weak_ptr<RequestContext>> config;
  config.number_of_workers = 1;
  return UdfClient::Create(std::move(config));
}

class UdfClientTest : public ::testing::Test {
 protected:
  UdfClientTest() {
    privacy_sandbox::server_common::log::ServerToken(
        kExampleConsentedDebugToken);
    InitMetricsContextMap();
    request_context_factory_ = std::make_unique<RequestContextFactory>(
        privacy_sandbox::server_common::LogContext(),
        privacy_sandbox::server_common::ConsentedDebugConfiguration());
  }
  std::unique_ptr<RequestContextFactory> request_context_factory_;
  ExecutionMetadata execution_metadata_;
};

TEST_F(UdfClientTest, UdfClient_Create_Success) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());
  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsCallSucceeds) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = () => 'Hello world!';",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world!")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsExceptionReturnsStatus) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "function hello() { throw new Error('Oh no!'); }",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.status().message(), HasSubstr("Oh no!"));

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, RepeatedJsCallsSucceed) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = () => 'Hello world!';",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result1 = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  EXPECT_TRUE(result1.ok());
  EXPECT_EQ(*result1, R"("Hello world!")");

  absl::StatusOr<std::string> result2 = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  EXPECT_TRUE(result2.ok());
  EXPECT_EQ(*result2, R"("Hello world!")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsEchoCallSucceeds) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (input) => 'Hello world! ' + JSON.stringify(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"("ECHO")"}, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! \"ECHO\"")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsEchoCallSucceeds_SimpleUDFArg_string) {
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
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, args, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! \"ECHO\"")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsEchoCallSucceeds_SimpleUDFArg_string_tagged) {
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
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, args, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result,
            R"("Hello world! {\"tags\":[\"tag1\"],\"data\":\"ECHO\"}")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}
TEST_F(UdfClientTest, JsEchoCallSucceeds_SimpleUDFArg_string_tagged_list) {
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
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, args, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(
      *result,
      R"("Hello world! {\"tags\":[\"tag1\"],\"data\":[\"key1\",\"key2\"]}")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsEchoCallSucceeds_SimpleUDFArg_struct) {
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
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, args, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! {\"key\":\"value\"}")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
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
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = "hello = (input) => 'Hello world! ' + echo(input);",
      .udf_handler_name = "hello",
      .logical_commit_time = 1,
      .version = 1,
  });
  EXPECT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"("I'm a key")"}, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Hello world! Echo: I'm a key")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
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
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"("key1")"}, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Key: key1, Value: value1")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
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
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("Key: key1, Value: value1")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
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
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"(["a"])");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, VerifyJsRunSetQueryIntHookSucceeds) {
  auto mock_lookup = std::make_unique<MockLookup>();
  InternalRunSetQueryIntResponse response;
  TextFormat::ParseFromString(R"pb(elements: 1000 elements: 1001)pb",
                              &response);
  ON_CALL(*mock_lookup, RunSetQueryInt(_, _)).WillByDefault(Return(response));
  auto run_query_hook = RunSetQueryIntHook::Create();
  run_query_hook->FinishInit(std::move(mock_lookup));
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      std::move(config_builder.RegisterRunSetQueryIntHook(*run_query_hook)
                    .SetNumberOfWorkers(1)
                    .Config()));
  EXPECT_TRUE(udf_client.ok());
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          let keys = input.keys;
          let bytes = runSetQueryInt(keys[0]);
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
  EXPECT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(*result, R"([1000,1001])");
  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsCallsLoggingFunctionLogForConsentedRequests) {
  std::stringstream log_ss;
  auto* logger_provider =
      opentelemetry::sdk::logs::LoggerProviderFactory::Create(
          opentelemetry::sdk::logs::SimpleLogRecordProcessorFactory::Create(
              std::make_unique<
                  opentelemetry::exporter::logs::OStreamLogRecordExporter>(
                  log_ss)))
          .release();
  privacy_sandbox::server_common::log::logger_private =
      logger_provider->GetLogger("test").get();
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(std::move(config_builder.RegisterLoggingFunction()
                                      .SetNumberOfWorkers(1)
                                      .Config()));
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          const a = console.error("Error message");
          const b = console.warn("Warning message");
          const c = console.log("Info message");
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
  EXPECT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("")");
  auto output_log = log_ss.str();
  EXPECT_THAT(output_log, ContainsRegex("Error message"));
  EXPECT_THAT(output_log, ContainsRegex("Warning message"));
  EXPECT_THAT(output_log, ContainsRegex("Info message"));

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, JsCallsLoggingFunctionNoLogForNonConsentedRequests) {
  std::stringstream log_ss;
  auto* logger_provider =
      opentelemetry::sdk::logs::LoggerProviderFactory::Create(
          opentelemetry::sdk::logs::SimpleLogRecordProcessorFactory::Create(
              std::make_unique<
                  opentelemetry::exporter::logs::OStreamLogRecordExporter>(
                  log_ss)))
          .release();
  privacy_sandbox::server_common::log::logger_private =
      logger_provider->GetLogger("test").get();
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(std::move(config_builder.RegisterLoggingFunction()
                                      .SetNumberOfWorkers(1)
                                      .Config()));
  EXPECT_TRUE(udf_client.ok());

  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(input) {
          const a = console.error("Error message");
          const b = console.warn("Warning message");
          const c = console.log("Info message");
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
  EXPECT_TRUE(code_obj_status.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {R"({"keys":["key1"]})"}, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("")");
  auto output_log = log_ss.str();
  EXPECT_TRUE(output_log.empty());
  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, UpdatesCodeObjectTwice) {
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
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("2")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, IgnoresCodeObjectWithSameCommitTime) {
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
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("1")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, IgnoresCodeObjectWithSmallerCommitTime) {
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
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("1")");

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, CodeObjectNotSetError) {
  auto udf_client = CreateUdfClient();
  EXPECT_TRUE(udf_client.ok());
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, {}, execution_metadata_);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);

  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

TEST_F(UdfClientTest, MetadataPassedSuccesfully) {
  UdfConfigBuilder config_builder;
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client = UdfClient::Create(
      std::move(config_builder.SetNumberOfWorkers(1).Config()));
  EXPECT_TRUE(udf_client.ok());
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = R"(
        function hello(metadata) {
          if(metadata.requestMetadata &&
              metadata.requestMetadata.is_pas)
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
  EXPECT_TRUE(code_obj_status.ok());
  v2::GetValuesRequest req;
  (*(req.mutable_metadata()->mutable_fields()))["is_pas"].set_string_value(
      "true");
  UDFExecutionMetadata udf_metadata;
  *udf_metadata.mutable_request_metadata() = *req.mutable_metadata();
  google::protobuf::RepeatedPtrField<UDFArgument> args;
  absl::StatusOr<std::string> result = udf_client.value()->ExecuteCode(
      *request_context_factory_, std::move(udf_metadata), args,
      execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("true")");

  UDFExecutionMetadata udf_metadata_non_pas;

  result = udf_client.value()->ExecuteCode(*request_context_factory_,
                                           std::move(udf_metadata_non_pas),
                                           args, execution_metadata_);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"("false")");
  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
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
  EXPECT_TRUE(udf_client.ok());
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = kDefaultUdfCodeSnippet,
      .udf_handler_name = kDefaultUdfHandlerName,
      .logical_commit_time = kDefaultLogicalCommitTime,
      .version = kDefaultVersion,
  });
  EXPECT_TRUE(code_obj_status.ok());
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
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(
      *result,
      R"({"keyGroupOutputs":[{"keyValues":{"key1":{"value":"value1"}}}],"udfOutputApiVersion":1})");
  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
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
                    .SetNumberOfWorkers(1)
                    .Config()));
  EXPECT_TRUE(udf_client.ok());
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = kDefaultUdfCodeSnippet,
      .udf_handler_name = kDefaultUdfHandlerName,
      .logical_commit_time = kDefaultLogicalCommitTime,
      .version = kDefaultVersion,
  });
  EXPECT_TRUE(code_obj_status.ok());
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
  EXPECT_FALSE(result.ok());
  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
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
                    .SetNumberOfWorkers(1)
                    .Config()));
  EXPECT_TRUE(udf_client.ok());
  absl::Status code_obj_status = udf_client.value()->SetCodeObject(CodeConfig{
      .js = kDefaultUdfCodeSnippet,
      .udf_handler_name = kDefaultUdfHandlerName,
      .logical_commit_time = kDefaultLogicalCommitTime,
      .version = kDefaultVersion,
  });
  EXPECT_TRUE(code_obj_status.ok());
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
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result, R"({"key1":{"value":"value1"}})");
  absl::Status stop = udf_client.value()->Stop();
  EXPECT_TRUE(stop.ok());
}

}  // namespace

}  // namespace kv_server
