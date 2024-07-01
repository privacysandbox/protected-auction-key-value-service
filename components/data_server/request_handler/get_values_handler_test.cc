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

#include "components/data_server/request_handler/get_values_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/key_value_cache.h"
#include "components/data_server/cache/mocks.h"
#include "components/data_server/request_handler/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "public/test_util/proto_matcher.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using grpc::StatusCode;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::ReturnRef;
using testing::SetArgReferee;
using testing::UnorderedElementsAre;
using v1::GetValuesRequest;
using v1::GetValuesResponse;

class GetValuesHandlerTest : public ::testing::Test {
 protected:
  GetValuesHandlerTest() {
    InitMetricsContextMap();
    request_context_factory_ = std::make_unique<RequestContextFactory>();
    request_context_factory_->UpdateLogContext(
        privacy_sandbox::server_common::LogContext(),
        privacy_sandbox::server_common::ConsentedDebugConfiguration());
  }
  MockCache mock_cache_;
  MockGetValuesAdapter mock_get_values_adapter_;
  RequestContextFactory& GetRequestContextFactory() {
    return *request_context_factory_;
  }
  std::unique_ptr<RequestContextFactory> request_context_factory_;
};

TEST_F(GetValuesHandlerTest, ReturnsExistingKeyTwice) {
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(_, UnorderedElementsAre("my_key")))
      .Times(2)
      .WillRepeatedly(Return(absl::flat_hash_map<std::string, std::string>{
          {"my_key", "my_value"}}));
  GetValuesRequest request;
  request.add_keys("my_key");
  GetValuesResponse response;
  GetValuesHandler handler(mock_cache_, mock_get_values_adapter_,
                           /*use_v2=*/false);
  const auto result =
      handler.GetValues(GetRequestContextFactory(), request, &response);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  GetValuesResponse expected;
  TextFormat::ParseFromString(
      R"pb(keys {
             key: "my_key"
             value { value { string_value: "my_value" } }
           })pb",
      &expected);
  EXPECT_THAT(response, EqualsProto(expected));

  ASSERT_TRUE(
      handler.GetValues(GetRequestContextFactory(), request, &response).ok());
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST_F(GetValuesHandlerTest, RepeatedKeys) {
  EXPECT_CALL(mock_cache_,
              GetKeyValuePairs(_, UnorderedElementsAre("key1", "key2", "key3")))
      .Times(1)
      .WillRepeatedly(Return(
          absl::flat_hash_map<std::string, std::string>{{"key1", "value1"}}));
  GetValuesRequest request;
  request.add_keys("key1,key2,key3");
  GetValuesResponse response;
  GetValuesHandler handler(mock_cache_, mock_get_values_adapter_,
                           /*use_v2=*/false);
  ASSERT_TRUE(
      handler.GetValues(GetRequestContextFactory(), request, &response).ok());

  GetValuesResponse expected;
  TextFormat::ParseFromString(
      R"pb(
        keys {
          key: "key1"
          value { value { string_value: "value1" } }
        }
        keys {
          key: "key2"
          value { status { code: 5 message: "Key not found" } }
        }
        keys {
          key: "key3"
          value { status { code: 5 message: "Key not found" } }
        }
      )pb",
      &expected);
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST_F(GetValuesHandlerTest, RepeatedKeysSkipEmpty) {
  EXPECT_CALL(mock_cache_,
              GetKeyValuePairs(_, UnorderedElementsAre("key1", "key2", "key3")))
      .Times(1)
      .WillRepeatedly(Return(
          absl::flat_hash_map<std::string, std::string>{{"key1", "value1"}}));
  GetValuesRequest request;
  request.add_keys("key1,key2,key3");
  GetValuesResponse response;
  GetValuesHandler handler(mock_cache_, mock_get_values_adapter_,
                           /*use_v2=*/false, /*add_missing_keys_v1=*/false);
  ASSERT_TRUE(
      handler.GetValues(GetRequestContextFactory(), request, &response).ok());

  GetValuesResponse expected;
  TextFormat::ParseFromString(
      R"pb(
        keys {
          key: "key1"
          value { value { string_value: "value1" } }
        }
      )pb",
      &expected);
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST_F(GetValuesHandlerTest, ReturnsMultipleExistingKeysSameNamespace) {
  EXPECT_CALL(mock_cache_,
              GetKeyValuePairs(_, UnorderedElementsAre("key1", "key2")))
      .Times(1)
      .WillOnce(Return(absl::flat_hash_map<std::string, std::string>{
          {"key1", "value1"}, {"key2", "value2"}}));
  GetValuesRequest request;
  request.add_keys("key1");
  request.add_keys("key2");
  GetValuesResponse response;
  GetValuesHandler handler(mock_cache_, mock_get_values_adapter_,
                           /*use_v2=*/false);
  ASSERT_TRUE(
      handler.GetValues(GetRequestContextFactory(), request, &response).ok());

  GetValuesResponse expected;
  TextFormat::ParseFromString(R"pb(
                                keys {
                                  key: "key1"
                                  value { value { string_value: "value1" } }
                                }
                                keys {
                                  key: "key2"
                                  value { value { string_value: "value2" } }
                                })pb",
                              &expected);
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST_F(GetValuesHandlerTest, ReturnsMultipleExistingKeysDifferentNamespace) {
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(_, UnorderedElementsAre("key1")))
      .Times(1)
      .WillOnce(Return(
          absl::flat_hash_map<std::string, std::string>{{"key1", "value1"}}));
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(_, UnorderedElementsAre("key2")))
      .Times(1)
      .WillOnce(Return(
          absl::flat_hash_map<std::string, std::string>{{"key2", "value2"}}));
  GetValuesRequest request;
  request.add_render_urls("key1");
  request.add_ad_component_render_urls("key2");
  GetValuesResponse response;
  GetValuesHandler handler(mock_cache_, mock_get_values_adapter_,
                           /*use_v2=*/false);
  ASSERT_TRUE(
      handler.GetValues(GetRequestContextFactory(), request, &response).ok());

  GetValuesResponse expected;
  TextFormat::ParseFromString(R"pb(render_urls {
                                     key: "key1"
                                     value { value { string_value: "value1" } }
                                   }
                                   ad_component_render_urls {
                                     key: "key2"
                                     value { value { string_value: "value2" } }
                                   })pb",
                              &expected);
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST_F(GetValuesHandlerTest, TestResponseOnDifferentValueFormats) {
  std::string value1 = R"json([
      [[1, 2, 3, 4]],
      null,
      [
        "123456789",
        "123456789",
      ],
      ["v1"]
    ])json";
  std::string value2 = R"json({
    "k1": 123,
    "k2": "v",
  })json";
  std::string value3("v3");

  std::string response_pb_string =
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
      value { value { string_value: "v3" } }
    })pb";

  std::string response_json_string = R"json({
    "keys":{
      "key1": { "value": [[[1,2,3,4]],null,["123456789","123456789"],["v1"]] },
      "key2":{ "value": {"k2":"v","k1":123} },
      "key3":{ "value": "v3" }
  })json";

  EXPECT_CALL(mock_cache_,
              GetKeyValuePairs(_, UnorderedElementsAre("key1", "key2", "key3")))
      .Times(1)
      .WillOnce(Return(absl::flat_hash_map<std::string, std::string>{
          {"key1", value1}, {"key2", value2}, {"key3", value3}}));

  GetValuesRequest request;
  request.add_keys("key1");
  request.add_keys("key2");
  request.add_keys("key3");
  GetValuesResponse response;
  GetValuesHandler handler(mock_cache_, mock_get_values_adapter_,
                           /*use_v2=*/false);
  ASSERT_TRUE(
      handler.GetValues(GetRequestContextFactory(), request, &response).ok());
  GetValuesResponse expected_from_pb;
  TextFormat::ParseFromString(response_pb_string, &expected_from_pb);
  EXPECT_THAT(response, EqualsProto(expected_from_pb));
  GetValuesResponse expected_from_json;
  google::protobuf::util::JsonStringToMessage(response_json_string,
                                              &expected_from_json);
  EXPECT_THAT(response, EqualsProto(expected_from_json));
}

TEST_F(GetValuesHandlerTest, CallsV2Adapter) {
  GetValuesResponse adapter_response;
  TextFormat::ParseFromString(R"pb(keys {
                                     fields {
                                       key: "key1"
                                       value { string_value: "value1" }
                                     }
                                   })pb",
                              &adapter_response);
  EXPECT_CALL(mock_get_values_adapter_, CallV2Handler(_, _, _))
      .WillOnce(
          DoAll(SetArgReferee<2>(adapter_response), Return(grpc::Status::OK)));

  GetValuesRequest request;
  request.add_keys("key1");
  GetValuesResponse response;
  GetValuesHandler handler(mock_cache_, mock_get_values_adapter_,
                           /*use_v2=*/true);
  ASSERT_TRUE(
      handler.GetValues(GetRequestContextFactory(), request, &response).ok());
  EXPECT_THAT(response, EqualsProto(adapter_response));
}

TEST_F(GetValuesHandlerTest, ReturnsPerInterestGroupData) {
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(_, UnorderedElementsAre("my_key")))
      .Times(2)
      .WillRepeatedly(Return(absl::flat_hash_map<std::string, std::string>{
          {"my_key", "my_value"}}));
  GetValuesRequest request;
  request.add_interest_group_names("my_key");
  GetValuesResponse response;
  GetValuesHandler handler(mock_cache_, mock_get_values_adapter_,
                           /*use_v2=*/false);
  const auto result =
      handler.GetValues(GetRequestContextFactory(), request, &response);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  GetValuesResponse expected;
  TextFormat::ParseFromString(
      R"pb(per_interest_group_data {
             key: "my_key"
             value { value { string_value: "my_value" } }
           })pb",
      &expected);
  EXPECT_THAT(response, EqualsProto(expected));

  ASSERT_TRUE(
      handler.GetValues(GetRequestContextFactory(), request, &response).ok());
  EXPECT_THAT(response, EqualsProto(expected));
}

}  // namespace
}  // namespace kv_server
