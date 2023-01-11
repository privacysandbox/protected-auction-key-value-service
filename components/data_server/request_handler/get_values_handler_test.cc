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
#include "components/data_server/cache/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "infrastructure/communication/bhttp_utils.h"
#include "public/test_util/proto_matcher.h"
#include "quiche/binary_http/binary_http_message.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using grpc::StatusCode;
using testing::Return;
using testing::ReturnRef;
using testing::UnorderedElementsAre;
using v1::GetValuesRequest;
using v1::GetValuesResponse;

enum class ProtocolType {
  kPlain = 0,
  kBinaryHttp = 1,
};

class GetValuesHandlerTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<ProtocolType> {
 protected:
  template <ProtocolType protocol_type>
  bool IsUsing() {
    return GetParam() == protocol_type;
  }

  grpc::Status GetValuesBasedOnProtocol(const v1::GetValuesRequest& request,
                                        v1::GetValuesResponse* response,
                                        GetValuesHandler* handler) {
    if (IsUsing<ProtocolType::kPlain>()) {
      return handler->GetValues(request, response);
    }
    v1::BinaryHttpGetValuesRequest brequest;
    brequest.mutable_raw_body()->set_data(
        SerializeProtoToBHttp<quiche::BinaryHttpRequest, GetValuesRequest>(
            request, {})
            .value());
    google::api::HttpBody bresponse;
    if (const auto s = handler->BinaryHttpGetValues(brequest, &bresponse);
        !s.ok()) {
      return s;
    }
    if (const auto maybe_response =
            DeserializeBHttpToProto<quiche::BinaryHttpResponse,
                                    GetValuesResponse>(bresponse.data());
        !maybe_response.ok()) {
      return grpc::Status(StatusCode::INTERNAL,
                          std::string(maybe_response.status().message()));
    } else {
      *response = *maybe_response;
    }

    return grpc::Status::OK;
  }

  MockShardedCache sharded_cache_;
  MockCache mock_cache_;
};

INSTANTIATE_TEST_SUITE_P(GetValuesHandlerTest, GetValuesHandlerTest,
                         testing::Values(ProtocolType::kPlain,
                                         ProtocolType::kBinaryHttp));

TEST_P(GetValuesHandlerTest, ReturnsExistingKeyTwice) {
  EXPECT_CALL(sharded_cache_, GetCacheShard(KeyNamespace::KEYS))
      .Times(2)
      .WillRepeatedly(ReturnRef(mock_cache_));
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(UnorderedElementsAre(FullKeyEq(
                               Cache::FullyQualifiedKey{.key = "my_key"}))))
      .Times(2)
      .WillRepeatedly(
          Return(std::vector<std::pair<Cache::FullyQualifiedKey, std::string>>{
              {{.key = "my_key"}, "my_value"}}));

  GetValuesRequest request;
  request.add_keys("my_key");
  GetValuesResponse response;
  GetValuesHandler handler(sharded_cache_, /*dsp_mode=*/true);
  const auto result = GetValuesBasedOnProtocol(request, &response, &handler);
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

  ASSERT_TRUE(GetValuesBasedOnProtocol(request, &response, &handler).ok());
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST_P(GetValuesHandlerTest, RepeatedKeys) {
  MockShardedCache sharded_cache_;
  MockCache mock_cache_;
  EXPECT_CALL(sharded_cache_, GetCacheShard(KeyNamespace::KEYS))
      .Times(1)
      .WillRepeatedly(ReturnRef(mock_cache_));
  EXPECT_CALL(mock_cache_,
              GetKeyValuePairs(UnorderedElementsAre(
                  FullKeyEq(Cache::FullyQualifiedKey{.key = "key1"}),
                  FullKeyEq(Cache::FullyQualifiedKey{.key = "key2"}),
                  FullKeyEq(Cache::FullyQualifiedKey{.key = "key3"}))))
      .Times(1)
      .WillRepeatedly(
          Return(std::vector<std::pair<Cache::FullyQualifiedKey, std::string>>{
              {{.key = "key1"}, "value1"}}));

  GetValuesRequest request;
  request.add_keys("key1,key2,key3");
  GetValuesResponse response;
  GetValuesHandler handler(sharded_cache_, /*dsp_mode=*/true);
  ASSERT_TRUE(GetValuesBasedOnProtocol(request, &response, &handler).ok());

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

TEST_P(GetValuesHandlerTest, ReturnsMultipleExistingKeysSameNamespace) {
  MockShardedCache sharded_cache_;
  MockCache mock_cache_;
  EXPECT_CALL(sharded_cache_, GetCacheShard(KeyNamespace::KEYS))
      .Times(1)
      .WillOnce(ReturnRef(mock_cache_));
  EXPECT_CALL(mock_cache_,
              GetKeyValuePairs(UnorderedElementsAre(
                  FullKeyEq(Cache::FullyQualifiedKey{.key = "key1"}),
                  FullKeyEq(Cache::FullyQualifiedKey{.key = "key2"}))))
      .Times(1)
      .WillOnce(
          Return(std::vector<std::pair<Cache::FullyQualifiedKey, std::string>>{
              {{.key = "key1"}, "value1"}, {{.key = "key2"}, "value2"}}));

  GetValuesRequest request;
  request.add_keys("key1");
  request.add_keys("key2");
  GetValuesResponse response;
  GetValuesHandler handler(sharded_cache_, /*dsp_mode=*/true);
  ASSERT_TRUE(GetValuesBasedOnProtocol(request, &response, &handler).ok());

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

TEST_P(GetValuesHandlerTest, ReturnsMultipleExistingKeysDifferentNamespace) {
  MockShardedCache sharded_cache_;
  MockCache render_urls_cache;
  EXPECT_CALL(sharded_cache_, GetCacheShard(KeyNamespace::RENDER_URLS))
      .Times(1)
      .WillOnce(ReturnRef(render_urls_cache));
  EXPECT_CALL(render_urls_cache,
              GetKeyValuePairs(UnorderedElementsAre(
                  FullKeyEq(Cache::FullyQualifiedKey{.key = "key1"}))))
      .Times(1)
      .WillOnce(
          Return(std::vector<std::pair<Cache::FullyQualifiedKey, std::string>>{
              {{.key = "key1"}, "value1"}}));
  MockCache component_url_cache;
  EXPECT_CALL(sharded_cache_,
              GetCacheShard(KeyNamespace::AD_COMPONENT_RENDER_URLS))
      .Times(1)
      .WillOnce(ReturnRef(component_url_cache));
  EXPECT_CALL(component_url_cache,
              GetKeyValuePairs(UnorderedElementsAre(
                  FullKeyEq(Cache::FullyQualifiedKey{.key = "key2"}))))
      .Times(1)
      .WillOnce(
          Return(std::vector<std::pair<Cache::FullyQualifiedKey, std::string>>{
              {{.key = "key2"}, "value2"}}));

  GetValuesRequest request;
  request.add_render_urls("key1");
  request.add_ad_component_render_urls("key2");
  GetValuesResponse response;
  GetValuesHandler handler(sharded_cache_, /*dsp_mode=*/false);
  ASSERT_TRUE(GetValuesBasedOnProtocol(request, &response, &handler).ok());

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

TEST_P(GetValuesHandlerTest, DspModeErrorOnMissingKeysNamespace) {
  std::unique_ptr<ShardedCache> cache = ShardedCache::Create();
  GetValuesRequest request;
  request.set_subkey("my_subkey");
  GetValuesResponse response;
  GetValuesHandler handler(*cache, /*dsp_mode=*/true);
  if (IsUsing<ProtocolType::kPlain>()) {
    grpc::Status status = handler.GetValues(request, &response);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(status.error_details(), "Missing field 'keys'");
  } else {
    grpc::Status status =
        GetValuesBasedOnProtocol(request, &response, &handler);
    EXPECT_FALSE(status.ok());
  }
}

TEST_P(GetValuesHandlerTest, ErrorOnMissingKeysInDspMode) {
  std::unique_ptr<ShardedCache> cache = ShardedCache::Create();
  GetValuesRequest request;
  GetValuesResponse response;
  GetValuesHandler handler(*cache, /*dsp_mode=*/true);
  if (IsUsing<ProtocolType::kPlain>()) {
    grpc::Status status = handler.GetValues(request, &response);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(status.error_details(), "Missing field 'keys'");
  } else {
    grpc::Status status =
        GetValuesBasedOnProtocol(request, &response, &handler);
    EXPECT_FALSE(status.ok());
  }
}

TEST_P(GetValuesHandlerTest, ErrorOnRenderUrlInDspMode) {
  std::unique_ptr<ShardedCache> cache = ShardedCache::Create();
  GetValuesRequest request;
  request.add_keys("my_key");
  request.add_render_urls("my_render_url");
  GetValuesResponse response;
  GetValuesHandler handler(*cache, /*dsp_mode=*/true);
  if (IsUsing<ProtocolType::kPlain>()) {
    grpc::Status status = handler.GetValues(request, &response);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(status.error_details(), "Invalid field 'renderUrls'");
  } else {
    grpc::Status status =
        GetValuesBasedOnProtocol(request, &response, &handler);
    EXPECT_FALSE(status.ok());
  }
}

TEST_P(GetValuesHandlerTest, ErrorOnAdComponentRenderUrlInDspMode) {
  std::unique_ptr<ShardedCache> cache = ShardedCache::Create();
  GetValuesRequest request;
  request.add_keys("my_key");
  request.add_ad_component_render_urls("my_ad_component_render_url");
  GetValuesResponse response;
  GetValuesHandler handler(*cache, /*dsp_mode=*/true);
  if (IsUsing<ProtocolType::kPlain>()) {
    grpc::Status status = handler.GetValues(request, &response);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(status.error_details(), "Invalid field 'adComponentRenderUrls'");
  } else {
    grpc::Status status =
        GetValuesBasedOnProtocol(request, &response, &handler);
    EXPECT_FALSE(status.ok());
  }
}

TEST_P(GetValuesHandlerTest, ErrorOnMissingRenderUrlInSspMode) {
  std::unique_ptr<ShardedCache> cache = ShardedCache::Create();
  GetValuesRequest request;
  request.add_ad_component_render_urls("my_ad_component_render_url");
  GetValuesResponse response;
  GetValuesHandler handler(*cache, /*dsp_mode=*/false);
  if (IsUsing<ProtocolType::kPlain>()) {
    grpc::Status status = handler.GetValues(request, &response);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(status.error_details(), "Missing field 'renderUrls'");
  } else {
    grpc::Status status =
        GetValuesBasedOnProtocol(request, &response, &handler);
    EXPECT_FALSE(status.ok());
  }
}

TEST_P(GetValuesHandlerTest, ErrorOnKeysInSspMode) {
  std::unique_ptr<ShardedCache> cache = ShardedCache::Create();
  GetValuesRequest request;
  request.add_render_urls("my_render_url");
  request.add_keys("my_key");
  GetValuesResponse response;
  GetValuesHandler handler(*cache, /*dsp_mode=*/false);
  if (IsUsing<ProtocolType::kPlain>()) {
    grpc::Status status = handler.GetValues(request, &response);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(status.error_details(), "Invalid field 'keys'");
  } else {
    grpc::Status status =
        GetValuesBasedOnProtocol(request, &response, &handler);
    EXPECT_FALSE(status.ok());
  }
}

TEST_P(GetValuesHandlerTest, ErrorOnSubkeysInSspMode) {
  std::unique_ptr<ShardedCache> cache = ShardedCache::Create();
  GetValuesRequest request;
  request.add_render_urls("my_render_url");
  request.set_subkey("my_subkey");
  GetValuesResponse response;
  GetValuesHandler handler(*cache, /*dsp_mode=*/false);
  if (IsUsing<ProtocolType::kPlain>()) {
    grpc::Status status = handler.GetValues(request, &response);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(status.error_details(), "Invalid field 'subkey'");
  } else {
    grpc::Status status =
        GetValuesBasedOnProtocol(request, &response, &handler);
    EXPECT_FALSE(status.ok());
  }
}

}  // namespace
}  // namespace kv_server
