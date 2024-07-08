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
#include "nlohmann/json.hpp"
#include "public/applications/pa/api_overlay.pb.h"
#include "public/applications/pa/response_utils.h"
#include "public/test_util/proto_matcher.h"
#include "src/encryption/key_fetcher/fake_key_fetcher_manager.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;

using testing::_;
using testing::Return;

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

class GetValuesAdapterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    v2_handler_ = std::make_unique<GetValuesV2Handler>(
        mock_udf_client_, fake_key_fetcher_manager_);
    get_values_adapter_ = GetValuesAdapter::Create(std::move(v2_handler_));
    InitMetricsContextMap();
    request_context_factory_ = std::make_unique<RequestContextFactory>();
  }

  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager_;
  std::unique_ptr<GetValuesAdapter> get_values_adapter_;
  std::unique_ptr<GetValuesV2Handler> v2_handler_;
  MockUdfClient mock_udf_client_;
  std::unique_ptr<RequestContextFactory> request_context_factory_;
};

TEST_F(GetValuesAdapterTest, EmptyRequestReturnsEmptyResponse) {
  UDFExecutionMetadata udf_metadata;
  TextFormat::ParseFromString(kEmptyMetadata, &udf_metadata);
  nlohmann::json output = nlohmann::json::parse(R"({"keyGroupOutputs": {}})");

  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(testing::_, EqualsProto(udf_metadata),
                          testing::IsEmpty(), testing::_))
      .WillOnce(Return(output.dump()));

  v1::GetValuesRequest v1_request;
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(*request_context_factory_,
                                                   v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb()pb", &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, V1RequestWithTwoKeysReturnsOk) {
  UDFExecutionMetadata udf_metadata;
  TextFormat::ParseFromString(kEmptyMetadata, &udf_metadata);
  UDFArgument arg;
  TextFormat::ParseFromString(R"(
tags {
  values {
    string_value: "custom"
  }
  values {
    string_value: "keys"
  }
}
data {
  list_value {
    values {
      string_value: "key1"
    }
    values {
      string_value: "key2"
    }
  }
})",
                              &arg);
  application_pa::KeyGroupOutputs key_group_outputs;
  TextFormat::ParseFromString(R"(
  key_group_outputs: {
    tags: "custom"
    tags: "keys"
    key_values: {
      key: "key1"
      value: {
        value: {
          string_value: "value1"
        }
      }
    }
    key_values: {
      key: "key2"
      value: {
        value: {
          string_value: "value2"
        }
      }
    }
  }
)",
                              &key_group_outputs);
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(testing::_, EqualsProto(udf_metadata),
                          testing::ElementsAre(EqualsProto(arg)), testing::_))
      .WillOnce(Return(
          application_pa::KeyGroupOutputsToJson(key_group_outputs).value()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1_request.add_keys("key2");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(*request_context_factory_,
                                                   v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(
      R"pb(
        keys {
        key: "key1"
        value {
          value {
            string_value: "value1"
          }
        }
      }
      keys {
        key: "key2"
        value {
          value {
            string_value: "value2"
          }
        }
      }
      })pb",
      &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, V1RequestSeparatesTwoKeysReturnsOk) {
  UDFExecutionMetadata udf_metadata;
  TextFormat::ParseFromString(kEmptyMetadata, &udf_metadata);
  UDFArgument arg;
  TextFormat::ParseFromString(R"(
tags {
  values {
    string_value: "custom"
  }
  values {
    string_value: "keys"
  }
}
data {
  list_value {
    values {
      string_value: "key1"
    }
    values {
      string_value: "key2"
    }
  }
})",
                              &arg);
  application_pa::KeyGroupOutputs key_group_outputs;
  TextFormat::ParseFromString(R"(
  key_group_outputs: {
    tags: "custom"
    tags: "keys"
    key_values: {
      key: "key1"
      value: {
        value: {
          string_value: "value1"
        }
      }
    }
    key_values: {
      key: "key2"
      value: {
        value: {
          string_value: "value2"
        }
      }
    }
  }
)",
                              &key_group_outputs);
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(testing::_, EqualsProto(udf_metadata),
                          testing::ElementsAre(EqualsProto(arg)), testing::_))
      .WillOnce(Return(
          application_pa::KeyGroupOutputsToJson(key_group_outputs).value()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1,key2");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(*request_context_factory_,
                                                   v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(
      R"pb(
        keys {
        key: "key1"
        value {
          value {
            string_value: "value1"
          }
        }
      }
      keys {
        key: "key2"
        value {
          value {
            string_value: "value2"
          }
        }
      }
      })pb",
      &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, V1RequestWithTwoKeyGroupsReturnsOk) {
  UDFExecutionMetadata udf_metadata;
  TextFormat::ParseFromString(kEmptyMetadata, &udf_metadata);
  UDFArgument arg1, arg2;
  TextFormat::ParseFromString(R"(
tags {
  values {
    string_value: "custom"
  }
  values {
    string_value: "renderUrls"
  }
}
data {
  list_value {
    values {
      string_value: "key1"
    }
  }
})",
                              &arg1);
  TextFormat::ParseFromString(R"(
tags {
  values {
    string_value: "custom"
  }
  values {
    string_value: "adComponentRenderUrls"
  }
}
data {
  list_value {
    values {
      string_value: "key2"
    }
  }
})",
                              &arg2);
  application_pa::KeyGroupOutputs key_group_outputs;
  TextFormat::ParseFromString(R"(
  key_group_outputs: {
    tags: "custom"
    tags: "renderUrls"
    key_values: {
      key: "key1"
      value: {
        value: {
          string_value: "value1"
        }
      }
    }
  }
  key_group_outputs: {
    tags: "custom"
    tags: "adComponentRenderUrls"
    key_values: {
      key: "key2"
      value: {
        value: {
          string_value: "value2"
        }
      }
    }
  }
)",
                              &key_group_outputs);
  EXPECT_CALL(
      mock_udf_client_,
      ExecuteCode(testing::_, EqualsProto(udf_metadata),
                  testing::ElementsAre(EqualsProto(arg1), EqualsProto(arg2)),
                  testing::_))
      .WillOnce(Return(
          application_pa::KeyGroupOutputsToJson(key_group_outputs).value()));

  v1::GetValuesRequest v1_request;
  v1_request.add_render_urls("key1");
  v1_request.add_ad_component_render_urls("key2");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(*request_context_factory_,
                                                   v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb(
                                render_urls {
                                  key: "key1"
                                  value { value { string_value: "value1" } }
                                }
                                ad_component_render_urls {
                                  key: "key2"
                                  value { value { string_value: "value2" } }
                                })pb",
                              &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, V2ResponseIsNullReturnsError) {
  nlohmann::json output = R"({
    "keyGroupOutpus": []
  })"_json;
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_, _, _, _))
      .WillOnce(Return(output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(*request_context_factory_,
                                                   v1_request, v1_response);
  EXPECT_FALSE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb()pb", &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, KeyGroupOutputWithEmptyKVsReturnsOk) {
  nlohmann::json output = R"({
    "keyGroupOutputs": [{
        "keyValues": {},
        "tags": ["custom","keys"]
    }],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_, _, _, _))
      .WillOnce(Return(output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(*request_context_factory_,
                                                   v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb()pb", &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, KeyGroupOutputWithInvalidNamespaceTagIsIgnored) {
  nlohmann::json output = R"({
    "keyGroupOutputs": [{
        "keyValues": { "key1": { "value": "value1" } },
        "tags": ["custom","invalidTag"]
    }],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_, _, _, _))
      .WillOnce(Return(output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(*request_context_factory_,
                                                   v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb()pb", &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, KeyGroupOutputWithNoCustomTagIsIgnored) {
  nlohmann::json output = R"({
    "keyGroupOutputs": [{
        "keyValues": { "key1": { "value": "value1" } },
        "tags": ["keys", "somethingelse"]
    }],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_, _, _, _))
      .WillOnce(Return(output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(*request_context_factory_,
                                                   v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb()pb", &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, KeyGroupOutputWithNoNamespaceTagIsIgnored) {
  nlohmann::json output = R"({
    "keyGroupOutputs": [{
        "keyValues": { "key1": { "value": "value1" } },
        "tags": ["custom"]
    }],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_, _, _, _))
      .WillOnce(Return(output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(*request_context_factory_,
                                                   v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb()pb", &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest,
       KeyGroupOutputHasDuplicateNamespaceTagReturnsAllKeys) {
  nlohmann::json output = R"({
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
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_, _, _, _))
      .WillOnce(Return(output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(*request_context_factory_,
                                                   v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb(
                                keys {
                                  key: "key1"
                                  value { value { string_value: "value1" } }
                                }
                                keys {
                                  key: "key2"
                                  value { value { string_value: "value2" } }
                                })pb",
                              &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, KeyGroupOutputHasDifferentValueTypesReturnsOk) {
  nlohmann::json output = R"({
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
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_, _, _, _))
      .WillOnce(Return(output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(*request_context_factory_,
                                                   v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(
      R"pb(
        keys {
          key: "key1"
          value {
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
        }
        keys {
          key: "key2"
          value {
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
        }
        keys {
          key: "key3"
          value { value { number_value: 3 } }
        })pb",
      &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

TEST_F(GetValuesAdapterTest, ValueWithStatusSuccess) {
  nlohmann::json output = R"({
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
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_, _, _, _))
      .WillOnce(Return(output.dump()));

  v1::GetValuesRequest v1_request;
  v1_request.add_keys("key1");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(*request_context_factory_,
                                                   v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(
      R"pb(
        keys {
          key: "key1"
          value {
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

TEST_F(GetValuesAdapterTest, V1RequestWithInterestGroupNamesReturnsOk) {
  UDFExecutionMetadata udf_metadata;
  TextFormat::ParseFromString(kEmptyMetadata, &udf_metadata);
  UDFArgument arg;
  TextFormat::ParseFromString(R"(
tags {
  values {
    string_value: "custom"
  }
  values {
    string_value: "interestGroupNames"
  }
}
data {
  list_value {
    values {
      string_value: "interestGroup1"
    }
    values {
      string_value: "interestGroup2"
    }
  }
})",
                              &arg);
  application_pa::KeyGroupOutputs key_group_outputs;
  TextFormat::ParseFromString(R"(
  key_group_outputs: {
    tags: "custom"
    tags: "interestGroupNames"
    key_values: {
      key: "interestGroup1"
      value: {
        value: {
          string_value: "value1"
        }
      }
    }
    key_values: {
      key: "interestGroup2"
      value: {
        value: {
          string_value: "{\"priorityVector\":{\"signal1\":1}}"
        }
      }
    }
  }
)",
                              &key_group_outputs);
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(testing::_, EqualsProto(udf_metadata),
                          testing::ElementsAre(EqualsProto(arg)), testing::_))
      .WillOnce(Return(
          application_pa::KeyGroupOutputsToJson(key_group_outputs).value()));

  v1::GetValuesRequest v1_request;
  v1_request.add_interest_group_names("interestGroup1");
  v1_request.add_interest_group_names("interestGroup2");
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(*request_context_factory_,
                                                   v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(
      R"pb(
        per_interest_group_data {
          key: "interestGroup1"
          value { value { string_value: "value1" } }
        }
        per_interest_group_data {
          key: "interestGroup2"
          value {
            value {
              struct_value {
                fields {
                  key: "priorityVector"
                  value {
                    struct_value {
                      fields {
                        key: "signal1"
                        value { number_value: 1 }
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
