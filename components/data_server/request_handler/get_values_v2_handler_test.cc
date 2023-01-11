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

#include "components/data_server/request_handler/get_values_v2_handler.h"

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
#include "nlohmann/json.hpp"
#include "public/test_util/proto_matcher.h"

namespace kv_server {
namespace {

using grpc::StatusCode;
using testing::Return;
using testing::ReturnRef;
using testing::UnorderedElementsAre;
using v2::GetValuesRequest;

// TODO(b/263284614): add bhttp tests
enum class ProtocolType {
  kPlain = 0,
};

class GetValuesHandlerTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<ProtocolType> {
 protected:
  template <ProtocolType protocol_type>
  bool IsUsing() {
    return GetParam() == protocol_type;
  }

  grpc::Status GetValuesBasedOnProtocol(const v2::GetValuesRequest& request,
                                        google::api::HttpBody* response,
                                        GetValuesV2Handler* handler) {
    return handler->GetValues(request, response);
    // TODO(b/263284614): add bhttp logic
  }

  MockShardedCache sharded_cache_;
  MockCache mock_cache_;
};

INSTANTIATE_TEST_SUITE_P(GetValuesHandlerTest, GetValuesHandlerTest,
                         testing::Values(ProtocolType::kPlain));

TEST_P(GetValuesHandlerTest, Success) {
  EXPECT_CALL(sharded_cache_, GetCacheShard(KeyNamespace::KV_INTERNAL))
      .Times(1)
      .WillRepeatedly(ReturnRef(mock_cache_));
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(UnorderedElementsAre(FullKeyEq(
                               Cache::FullyQualifiedKey{.key = "hello"}))))
      .Times(1)
      .WillRepeatedly(
          Return(std::vector<std::pair<Cache::FullyQualifiedKey, std::string>>{
              {{.key = "hello"}, "world"}}));
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(UnorderedElementsAre(FullKeyEq(
                               Cache::FullyQualifiedKey{.key = "hello2"}))))
      .Times(1)
      .WillRepeatedly(
          Return(std::vector<std::pair<Cache::FullyQualifiedKey, std::string>>{
              {{.key = "hello2"}, "world2"}}));

  GetValuesRequest request;
  request.mutable_raw_body()->set_data(R"(
{
  "context": {
    "subkey": "example.com"
  },
  "partitions": [
    {
      "id": 0,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "structured",
            "groupNames"
          ],
          "keyList": [
            "hello"
          ]
        },
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    },
    {
      "id": 1,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello2"
          ]
        }
      ]
    }
  ]
}
  )");
  google::api::HttpBody response;
  GetValuesV2Handler handler(sharded_cache_);
  const auto result = GetValuesBasedOnProtocol(request, &response, &handler);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  nlohmann::json expected = nlohmann::json::parse(R"(
{
  "0": {
    "partitions": [
      {
        "id": 0,
        "keyGroupOutputs": [
          {
            "keyValues": {
              "hello": {
                "value": "world"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
          }
        ]
      },
      {
        "id": 1,
        "keyGroupOutputs": [
          {
            "keyValues": {
              "hello2": {
                "value": "world2"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
          }
        ]
      }
    ]
  }
})");
  EXPECT_EQ(response.data(), expected.dump());
}
TEST_P(GetValuesHandlerTest, InvalidFormat) {
  GetValuesRequest request;
  request.mutable_raw_body()->set_data(R"(
{
  "context": {
    "subkey": "example.com"
  },
}
  )");
  google::api::HttpBody response;
  GetValuesV2Handler handler(sharded_cache_);
  const auto result = GetValuesBasedOnProtocol(request, &response, &handler);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
}

TEST_P(GetValuesHandlerTest, NotADictionary) {
  EXPECT_CALL(sharded_cache_, GetCacheShard(KeyNamespace::KV_INTERNAL))
      .Times(1)
      .WillRepeatedly(ReturnRef(mock_cache_));

  GetValuesRequest request;
  request.mutable_raw_body()->set_data(R"(
[]
  )");
  google::api::HttpBody response;
  GetValuesV2Handler handler(sharded_cache_);
  const auto result = GetValuesBasedOnProtocol(request, &response, &handler);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
}

TEST_P(GetValuesHandlerTest, NoPartition) {
  EXPECT_CALL(sharded_cache_, GetCacheShard(KeyNamespace::KV_INTERNAL))
      .Times(1)
      .WillRepeatedly(ReturnRef(mock_cache_));

  GetValuesRequest request;
  request.mutable_raw_body()->set_data(R"(
{
  "context": {
    "subkey": "example.com"
  }
}
  )");
  google::api::HttpBody response;
  GetValuesV2Handler handler(sharded_cache_);
  const auto result = GetValuesBasedOnProtocol(request, &response, &handler);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
}

TEST_P(GetValuesHandlerTest, NoContext) {
  EXPECT_CALL(sharded_cache_, GetCacheShard(KeyNamespace::KV_INTERNAL))
      .Times(1)
      .WillRepeatedly(ReturnRef(mock_cache_));

  GetValuesRequest request;
  request.mutable_raw_body()->set_data(R"(
{
  "partitions": [
    {
      "id": 0,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "structured",
            "groupNames"
          ],
          "keyList": [
            "hello"
          ]
        },
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    }
  ]

}
  )");
  google::api::HttpBody response;
  GetValuesV2Handler handler(sharded_cache_);
  const auto result = GetValuesBasedOnProtocol(request, &response, &handler);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
}

TEST_P(GetValuesHandlerTest, NoCompressionGroup) {
  EXPECT_CALL(sharded_cache_, GetCacheShard(KeyNamespace::KV_INTERNAL))
      .Times(1)
      .WillRepeatedly(ReturnRef(mock_cache_));

  GetValuesRequest request;
  request.mutable_raw_body()->set_data(R"(
{
  "context": {
    "subkey": "example.com"
  },
  "partitions": [
    {
      "id": 0,
      "keyGroups": [
        {
          "tags": [
            "structured",
            "groupNames"
          ],
          "keyList": [
            "hello"
          ]
        },
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    }
  ]

}
  )");
  google::api::HttpBody response;
  GetValuesV2Handler handler(sharded_cache_);
  const auto result = GetValuesBasedOnProtocol(request, &response, &handler);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
}

TEST_P(GetValuesHandlerTest, CompressionGroupNotANumber) {
  EXPECT_CALL(sharded_cache_, GetCacheShard(KeyNamespace::KV_INTERNAL))
      .Times(1)
      .WillRepeatedly(ReturnRef(mock_cache_));

  GetValuesRequest request;
  request.mutable_raw_body()->set_data(R"(
{
  "context": {
    "subkey": "example.com"
  },
  "partitions": [
    {
      "id": 0,
      "compressionGroup": "0",
      "keyGroups": [
        {
          "tags": [
            "structured",
            "groupNames"
          ],
          "keyList": [
            "hello"
          ]
        },
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    }
  ]

}
  )");
  google::api::HttpBody response;
  GetValuesV2Handler handler(sharded_cache_);
  const auto result = GetValuesBasedOnProtocol(request, &response, &handler);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
}

}  // namespace
}  // namespace kv_server
