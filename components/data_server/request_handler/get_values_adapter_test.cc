// Copyright 2023 Google LLC
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

#include "components/data_server/request_handler/get_values_adapter.h"

#include <string>
#include <utility>
#include <vector>

#include "components/udf/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "public/test_util/proto_matcher.h"
#include "src/cpp/telemetry/metrics_recorder.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;

using privacy_sandbox::server_common::MockMetricsRecorder;
using testing::_;
using testing::Return;

class GetValuesAdapterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    v2_handler_ = std::make_unique<GetValuesV2Handler>(mock_udf_client_,
                                                       mock_metrics_recorder_);
    get_values_adapter_ = GetValuesAdapter::Create(std::move(v2_handler_));
  }

  std::unique_ptr<GetValuesAdapter> get_values_adapter_;
  std::unique_ptr<GetValuesV2Handler> v2_handler_;
  MockUdfClient mock_udf_client_;
  MockMetricsRecorder mock_metrics_recorder_;
};

TEST_F(GetValuesAdapterTest, EmptyRequestReturnsEmptyResponse) {
  nlohmann::json udf_input =
      R"({"context":{"subkey":""},"keyGroups":[],"udfInputApiVersion":1})"_json;
  nlohmann::json udf_output =
      R"({"keyGroupOutputs": [], "udfOutputApiVersion": 1})"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input.dump()})))
      .WillOnce(Return(udf_output.dump()));

  v1::GetValuesRequest v1_request;
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb()pb", &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, V1RequestWithTwoKeysReturnsOk) {
  nlohmann::json udf_input = R"({
    "context": {"subkey": ""},
    "keyGroups": [{"tags": ["custom","keys"],"keyList": ["key1", "key2"]}],
    "udfInputApiVersion": 1
  })"_json;

  nlohmann::json udf_output = R"({
    "keyGroupOutputs": [{
        "keyValues": {
            "key1": { "value": "value1" },
            "key2": { "value": "value2" }
        },
        "tags": ["custom","keys"]
    }],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input.dump()})))
      .WillOnce(Return(udf_output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1_request.add_keys("key2");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb(
                                keys {
                                  fields {
                                    key: "key1"
                                    value { string_value: "value1" }
                                  }
                                  fields {
                                    key: "key2"
                                    value { string_value: "value2" }
                                  }

                                })pb",
                              &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, V1RequestWithTwoKeyGroupsReturnsOk) {
  nlohmann::json udf_input = R"({
    "context": {"subkey": ""},
    "keyGroups": [{"tags": ["custom","renderUrls"],"keyList": ["key1"]},{"keyList":["key2"],"tags":["custom","adComponentRenderUrls"]}],
    "udfInputApiVersion": 1
  })"_json;

  nlohmann::json udf_output = R"({
    "keyGroupOutputs": [{
        "keyValues": { "key1": { "value": "value1" } },
        "tags": ["custom","renderUrls"]
    },{
        "keyValues": { "key2": { "value": "value2" } },
        "tags": ["custom","adComponentRenderUrls"]
    }],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input.dump()})))
      .WillOnce(Return(udf_output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_render_urls("key1");
  v1_request.add_ad_component_render_urls("key2");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb(
                                render_urls {
                                  fields {
                                    key: "key1"
                                    value { string_value: "value1" }
                                  }
                                }
                                ad_component_render_urls {
                                  fields {
                                    key: "key2"
                                    value { string_value: "value2" }
                                  }
                                })pb",
                              &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, V2ResponseIsNullReturnsError) {
  nlohmann::json udf_input = R"({
    "context": {"subkey": ""},
    "keyGroups": [{"tags": ["custom","keys"],"keyList": ["key1"]}],
    "udfInputApiVersion": 1
  })"_json;

  nlohmann::json udf_output = R"({
    "keyGroupOutpus": []
  })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input.dump()})))
      .WillOnce(Return(udf_output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(v1_request, v1_response);
  EXPECT_FALSE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb()pb", &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, KeyGroupOutputWithEmptyKVsReturnsOk) {
  nlohmann::json udf_input = R"({
    "context": {"subkey": ""},
    "keyGroups": [{"tags": ["custom","keys"],"keyList": ["key1"]}],
    "udfInputApiVersion": 1
  })"_json;

  nlohmann::json udf_output = R"({
    "keyGroupOutputs": [{
        "keyValues": {},
        "tags": ["custom","keys"]
    }],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input.dump()})))
      .WillOnce(Return(udf_output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb(keys {})pb", &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, KeyGroupOutputWithInvalidNamespaceTagIsIgnored) {
  nlohmann::json udf_input = R"({
    "context": {"subkey": ""},
    "keyGroups": [{"tags": ["custom","keys"],"keyList": ["key1"]}],
    "udfInputApiVersion": 1
  })"_json;

  nlohmann::json udf_output = R"({
    "keyGroupOutputs": [{
        "keyValues": { "key1": { "value": "value1" } },
        "tags": ["custom","invalidTag"]
    }],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input.dump()})))
      .WillOnce(Return(udf_output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb()pb", &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, KeyGroupOutputWithNoCustomTagIsIgnored) {
  nlohmann::json udf_input = R"({
    "context": {"subkey": ""},
    "keyGroups": [{"tags": ["custom","keys"],"keyList": ["key1"]}],
    "udfInputApiVersion": 1
  })"_json;

  nlohmann::json udf_output = R"({
    "keyGroupOutputs": [{
        "keyValues": { "key1": { "value": "value1" } },
        "tags": ["keys", "somethingelse"]
    }],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input.dump()})))
      .WillOnce(Return(udf_output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb()pb", &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, KeyGroupOutputWithNoNamespaceTagIsIgnored) {
  nlohmann::json udf_input = R"({
    "context": {"subkey": ""},
    "keyGroups": [{"tags": ["custom","keys"],"keyList": ["key1"]}],
    "udfInputApiVersion": 1
  })"_json;

  nlohmann::json udf_output = R"({
    "keyGroupOutputs": [{
        "keyValues": { "key1": { "value": "value1" } },
        "tags": ["custom"]
    }],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input.dump()})))
      .WillOnce(Return(udf_output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb()pb", &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest,
       KeyGroupOutputHasDuplicateNamespaceTagReturnsAllKeys) {
  nlohmann::json udf_input = R"({
    "context": {"subkey": ""},
    "keyGroups": [{"tags": ["custom","keys"],"keyList": ["key1"]}],
    "udfInputApiVersion": 1
  })"_json;

  nlohmann::json udf_output = R"({
    "keyGroupOutputs": [{
        "keyValues": { "key1": { "value": "value1" } },
        "tags": ["custom", "keys"]
    },
    {
        "keyValues": { "key2": { "value": "value2" } },
        "tags": ["custom", "keys"]
    }],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input.dump()})))
      .WillOnce(Return(udf_output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb(
                                keys {
                                  fields {
                                    key: "key1"
                                    value { string_value: "value1" }
                                  }
                                  fields {
                                    key: "key2"
                                    value { string_value: "value2" }
                                  }
                                })pb",
                              &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, KeyGroupOutputHasDifferentValueTypesReturnsOk) {
  nlohmann::json udf_input = R"({
    "context": {"subkey": ""},
    "keyGroups": [{"tags": ["custom","keys"],"keyList": ["key1"]}],
    "udfInputApiVersion": 1
  })"_json;

  nlohmann::json udf_output = R"({
    "keyGroupOutputs": [{
        "keyValues": {
          "key1": { "value": [[[1,2,3,4]],null,["123456789","123456789"],["v1"]] },
          "key2": { "value": {"k2":"v","k1":123} },
          "key3": { "value": "3"}
        },
        "tags": ["custom", "keys"]
    }],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input.dump()})))
      .WillOnce(Return(udf_output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(
      R"pb(keys {
             fields {
               key: "key1"
               value {
                 list_value {
                   values {
                     list_value {
                       values {
                         list_value {
                           values { number_value: 1 }
                           values { number_value: 2 }
                           values { number_value: 3 }
                           values { number_value: 4 }
                         }
                       }
                     }
                   }
                   values { null_value: NULL_VALUE }
                   values {
                     list_value {
                       values { string_value: "123456789" }
                       values { string_value: "123456789" }
                     }
                   }
                   values { list_value { values { string_value: "v1" } } }
                 }
               }
             }
             fields {
               key: "key2"
               value {
                 struct_value {
                   fields {
                     key: "k1"
                     value { number_value: 123 }
                   }
                   fields {
                     key: "k2"
                     value { string_value: "v" }
                   }
                 }
               }
             }
             fields {
               key: "key3"
               value { string_value: "3" }
             }
           })pb",
      &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, ValueWithStatusSuccess) {
  nlohmann::json udf_input = R"({
    "context": {"subkey": ""},
    "keyGroups": [{"tags": ["custom","keys"],"keyList": ["key1"]}],
    "udfInputApiVersion": 1
  })"_json;

  nlohmann::json udf_output = R"({
    "keyGroupOutputs": [{
        "keyValues": { "key1": {
            "value": {
                "status": {
                    "code": 1,
                    "message": "some error message"
                }
            }
        } },
        "tags": ["custom", "keys"]
    }],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input.dump()})))
      .WillOnce(Return(udf_output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(
      R"pb(keys {
             fields {
               key: "key1"
               value {
                 struct_value {
                   fields {
                     key: "status"
                     value {
                       struct_value {
                         fields {
                           key: "code"
                           value { number_value: 1 }
                         }
                         fields {
                           key: "message"
                           value { string_value: "some error message" }
                         }
                       }
                     }
                   }
                 }
               }
             }
           })pb",
      &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

}  // namespace
}  // namespace kv_server
