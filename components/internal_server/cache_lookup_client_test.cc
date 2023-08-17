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

#include "components/internal_server/cache_lookup_client.h"

#include <string>

#include "absl/status/status.h"
#include "components/data_server/cache/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "public/test_util/proto_matcher.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using testing::_;
using testing::Return;

TEST(CacheLookupClientTest, GetValues_ReturnsKeysInCache) {
  MockCache cache;
  EXPECT_CALL(cache, GetKeyValuePairs(_))
      .WillOnce(Return(absl::flat_hash_map<std::string, std::string>{
          {"key1", "value1"}, {"key2", "value2"}}));
  auto cache_lookup_client = CreateCacheLookupClient(cache);

  auto response = cache_lookup_client->GetValues({"key1", "key2"});
  InternalLookupResponse expected;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   }
                                   kv_pairs {
                                     key: "key2"
                                     value { value: "value2" }
                                   }
                              )pb",
                              &expected);
  EXPECT_TRUE(response.ok());
  EXPECT_THAT(*response, EqualsProto(expected));
}

TEST(CacheLookupClientTest, GetValues_ReturnsStatusForEmptyKeyInCache) {
  MockCache cache;
  EXPECT_CALL(cache, GetKeyValuePairs(_))
      .WillOnce(Return(
          absl::flat_hash_map<std::string, std::string>{{"key1", "value1"}}));
  auto cache_lookup_client = CreateCacheLookupClient(cache);

  auto response = cache_lookup_client->GetValues({"key1", "key2"});
  InternalLookupResponse expected;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   }
                                   kv_pairs {
                                     key: "key2"
                                     value { status { code: 5 } }
                                   }
                              )pb",
                              &expected);
  EXPECT_TRUE(response.ok());
  EXPECT_THAT(*response, EqualsProto(expected));
}

}  // namespace
}  // namespace kv_server
