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
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "public/test_util/proto_matcher.h"
#include "src/cpp/telemetry/metrics_recorder.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using grpc::StatusCode;
using privacy_sandbox::server_common::MockMetricsRecorder;
using testing::Return;
using testing::ReturnRef;
using testing::UnorderedElementsAre;
using v1::GetValuesRequest;
using v1::GetValuesResponse;

class GetValuesHandlerTest : public ::testing::Test {
 protected:
  MockCache mock_cache_;
  MockMetricsRecorder mock_metrics_recorder_;
};

TEST_F(GetValuesHandlerTest, ReturnsExistingKeyTwice) {
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(UnorderedElementsAre("my_key")))
      .Times(2)
      .WillRepeatedly(Return(absl::flat_hash_map<std::string, std::string>{
          {"my_key", "my_value"}}));
  GetValuesRequest request;
  request.add_keys("my_key");
  GetValuesResponse response;
  GetValuesHandler handler(mock_cache_, mock_metrics_recorder_,
                           /*dsp_mode=*/true);
  const auto result = handler.GetValues(request, &response);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  GetValuesResponse expected;
  TextFormat::ParseFromString(R"pb(keys {
                                     fields {
                                       key: "my_key"
                                       value { string_value: "my_value" }
                                     }
                                   })pb",
                              &expected);
  EXPECT_THAT(response, EqualsProto(expected));

  ASSERT_TRUE(handler.GetValues(request, &response).ok());
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST_F(GetValuesHandlerTest, RepeatedKeys) {
  MockCache mock_cache_;
  EXPECT_CALL(mock_cache_,
              GetKeyValuePairs(UnorderedElementsAre("key1", "key2", "key3")))
      .Times(1)
      .WillRepeatedly(Return(
          absl::flat_hash_map<std::string, std::string>{{"key1", "value1"}}));
  GetValuesRequest request;
  request.add_keys("key1,key2,key3");
  GetValuesResponse response;
  GetValuesHandler handler(mock_cache_, mock_metrics_recorder_,
                           /*dsp_mode=*/true);
  ASSERT_TRUE(handler.GetValues(request, &response).ok());

  GetValuesResponse expected;
  TextFormat::ParseFromString(R"pb(keys {
                                     fields {
                                       key: "key1"
                                       value { string_value: "value1" }
                                     }
                                   })pb",
                              &expected);
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST_F(GetValuesHandlerTest, ReturnsMultipleExistingKeysSameNamespace) {
  MockCache mock_cache;
  EXPECT_CALL(mock_cache_,
              GetKeyValuePairs(UnorderedElementsAre("key1", "key2")))
      .Times(1)
      .WillOnce(Return(absl::flat_hash_map<std::string, std::string>{
          {"key1", "value1"}, {"key2", "value2"}}));
  GetValuesRequest request;
  request.add_keys("key1");
  request.add_keys("key2");
  GetValuesResponse response;
  GetValuesHandler handler(mock_cache_, mock_metrics_recorder_,
                           /*dsp_mode=*/true);
  ASSERT_TRUE(handler.GetValues(request, &response).ok());

  GetValuesResponse expected;
  TextFormat::ParseFromString(R"pb(keys {
                                     fields {
                                       key: "key1"
                                       value { string_value: "value1" }
                                     }
                                     fields {
                                       key: "key2"
                                       value { string_value: "value2" }
                                     }
                                   })pb",
                              &expected);
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST_F(GetValuesHandlerTest, ReturnsMultipleExistingKeysDifferentNamespace) {
  MockCache mock_cache;
  EXPECT_CALL(mock_cache, GetKeyValuePairs(UnorderedElementsAre("key1")))
      .Times(1)
      .WillOnce(Return(
          absl::flat_hash_map<std::string, std::string>{{"key1", "value1"}}));
  EXPECT_CALL(mock_cache, GetKeyValuePairs(UnorderedElementsAre("key2")))
      .Times(1)
      .WillOnce(Return(
          absl::flat_hash_map<std::string, std::string>{{"key2", "value2"}}));
  GetValuesRequest request;
  request.add_render_urls("key1");
  request.add_ad_component_render_urls("key2");
  GetValuesResponse response;
  GetValuesHandler handler(mock_cache, mock_metrics_recorder_,
                           /*dsp_mode=*/false);
  ASSERT_TRUE(handler.GetValues(request, &response).ok());

  GetValuesResponse expected;
  TextFormat::ParseFromString(R"pb(render_urls {
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
                              &expected);
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST_F(GetValuesHandlerTest, DspModeErrorOnMissingKeysNamespace) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  GetValuesRequest request;
  request.set_subkey("my_subkey");
  GetValuesResponse response;
  GetValuesHandler handler(*cache, mock_metrics_recorder_, /*dsp_mode=*/true);
  grpc::Status status = handler.GetValues(request, &response);
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(status.error_details(), "Missing field 'keys'");
}

TEST_F(GetValuesHandlerTest, ErrorOnMissingKeysInDspMode) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  GetValuesRequest request;
  GetValuesResponse response;
  GetValuesHandler handler(*cache, mock_metrics_recorder_, /*dsp_mode=*/true);
  grpc::Status status = handler.GetValues(request, &response);
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(status.error_details(), "Missing field 'keys'");
}

TEST_F(GetValuesHandlerTest, ErrorOnRenderUrlInDspMode) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  GetValuesRequest request;
  request.add_keys("my_key");
  request.add_render_urls("my_render_url");
  GetValuesResponse response;
  GetValuesHandler handler(*cache, mock_metrics_recorder_, /*dsp_mode=*/true);

  grpc::Status status = handler.GetValues(request, &response);
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(status.error_details(), "Invalid field 'renderUrls'");
}

TEST_F(GetValuesHandlerTest, ErrorOnAdComponentRenderUrlInDspMode) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  GetValuesRequest request;
  request.add_keys("my_key");
  request.add_ad_component_render_urls("my_ad_component_render_url");
  GetValuesResponse response;
  GetValuesHandler handler(*cache, mock_metrics_recorder_, /*dsp_mode=*/true);

  grpc::Status status = handler.GetValues(request, &response);
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(status.error_details(), "Invalid field 'adComponentRenderUrls'");
}

TEST_F(GetValuesHandlerTest, ErrorOnMissingRenderUrlInSspMode) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  GetValuesRequest request;
  request.add_ad_component_render_urls("my_ad_component_render_url");
  GetValuesResponse response;
  GetValuesHandler handler(*cache, mock_metrics_recorder_, /*dsp_mode=*/false);

  grpc::Status status = handler.GetValues(request, &response);
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(status.error_details(), "Missing field 'renderUrls'");
}

TEST_F(GetValuesHandlerTest, ErrorOnKeysInSspMode) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  GetValuesRequest request;
  request.add_render_urls("my_render_url");
  request.add_keys("my_key");
  GetValuesResponse response;
  GetValuesHandler handler(*cache, mock_metrics_recorder_, /*dsp_mode=*/false);

  grpc::Status status = handler.GetValues(request, &response);
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(status.error_details(), "Invalid field 'keys'");
}

TEST_F(GetValuesHandlerTest, ErrorOnSubkeysInSspMode) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  GetValuesRequest request;
  request.add_render_urls("my_render_url");
  request.set_subkey("my_subkey");
  GetValuesResponse response;
  GetValuesHandler handler(*cache, mock_metrics_recorder_, /*dsp_mode=*/false);

  grpc::Status status = handler.GetValues(request, &response);
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(status.error_details(), "Invalid field 'subkey'");
}

TEST_F(GetValuesHandlerTest, TestResponseOnDifferentValueFormats) {
  MockCache mock_cache;
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
               value { string_value: "v3" }
             }
           }
      )pb";

  std::string response_json_string = R"json({
    "keys":{
      "key1":[[[1,2,3,4]],null,["123456789","123456789"],["v1"]],
      "key2":{"k2":"v","k1":123},
      "key3":"v3"}
  })json";

  EXPECT_CALL(mock_cache_,
              GetKeyValuePairs(UnorderedElementsAre("key1", "key2", "key3")))
      .Times(1)
      .WillOnce(Return(absl::flat_hash_map<std::string, std::string>{
          {"key1", value1}, {"key2", value2}, {"key3", value3}}));

  GetValuesRequest request;
  request.add_keys("key1");
  request.add_keys("key2");
  request.add_keys("key3");
  GetValuesResponse response;
  GetValuesHandler handler(mock_cache_, mock_metrics_recorder_,
                           /*dsp_mode=*/true);
  ASSERT_TRUE(handler.GetValues(request, &response).ok());
  GetValuesResponse expected_from_pb;
  TextFormat::ParseFromString(response_pb_string, &expected_from_pb);
  EXPECT_THAT(response, EqualsProto(expected_from_pb));
  GetValuesResponse expected_from_json;
  google::protobuf::util::JsonStringToMessage(response_json_string,
                                              &expected_from_json);
  EXPECT_THAT(response, EqualsProto(expected_from_json));
}

}  // namespace
}  // namespace kv_server
