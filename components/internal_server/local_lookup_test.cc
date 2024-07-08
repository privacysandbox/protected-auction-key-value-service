// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/internal_server/local_lookup.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

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
using testing::ReturnRef;

class LocalLookupTest : public ::testing::Test {
 protected:
  LocalLookupTest() {
    InitMetricsContextMap();
    request_context_ = std::make_unique<RequestContext>();
    request_context_->UpdateLogContext(
        privacy_sandbox::server_common::LogContext(),
        privacy_sandbox::server_common::ConsentedDebugConfiguration());
  }
  RequestContext& GetRequestContext() { return *request_context_; }
  std::shared_ptr<RequestContext> request_context_;
  MockCache mock_cache_;
};

TEST_F(LocalLookupTest, GetKeyValues_KeysFound_Success) {
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(_, _))
      .WillOnce(Return(absl::flat_hash_map<std::string, std::string>{
          {"key1", "value1"}, {"key2", "value2"}}));

  auto local_lookup = CreateLocalLookup(mock_cache_);
  auto response =
      local_lookup->GetKeyValues(GetRequestContext(), {"key1", "key2"});
  EXPECT_TRUE(response.ok());

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
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(LocalLookupTest, GetKeyValues_DuplicateKeys_Success) {
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(_, _))
      .WillOnce(Return(absl::flat_hash_map<std::string, std::string>{
          {"key1", "value1"}, {"key2", "value2"}}));

  auto local_lookup = CreateLocalLookup(mock_cache_);
  auto response =
      local_lookup->GetKeyValues(GetRequestContext(), {"key1", "key1"});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "key1"
                                     value { value: "value1" }
                                   }
                              )pb",
                              &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(LocalLookupTest, GetKeyValues_KeyMissing_ReturnsStatusForKey) {
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(_, _))
      .WillOnce(Return(
          absl::flat_hash_map<std::string, std::string>{{"key1", "value1"}}));

  auto local_lookup = CreateLocalLookup(mock_cache_);
  auto response =
      local_lookup->GetKeyValues(GetRequestContext(), {"key1", "key2"});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { value: "value1" }
           }
           kv_pairs {
             key: "key2"
             value { status { code: 5 message: "Key not found: key2" } }
           }
      )pb",
      &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(LocalLookupTest, GetKeyValues_EmptyRequest_ReturnsEmptyResponse) {
  auto local_lookup = CreateLocalLookup(mock_cache_);
  auto response = local_lookup->GetKeyValues(GetRequestContext(), {});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(LocalLookupTest, GetKeyValueSets_KeysFound_Success) {
  auto mock_get_key_value_set_result =
      std::make_unique<MockGetKeyValueSetResult>();
  EXPECT_CALL(*mock_get_key_value_set_result, GetValueSet("key1"))
      .WillOnce(
          Return(absl::flat_hash_set<std::string_view>{"value1", "value2"}));
  EXPECT_CALL(mock_cache_, GetKeyValueSet(_, _))
      .WillOnce(Return(std::move(mock_get_key_value_set_result)));

  auto local_lookup = CreateLocalLookup(mock_cache_);
  auto response = local_lookup->GetKeyValueSet(GetRequestContext(), {"key1"});
  EXPECT_TRUE(response.ok());

  std::vector<std::string> expected_resulting_set = {"value1", "value2"};
  EXPECT_THAT(response.value().kv_pairs().at("key1").keyset_values().values(),
              testing::UnorderedElementsAreArray(expected_resulting_set));
}

TEST_F(LocalLookupTest, GetUInt32ValueSets_KeysFound_Success) {
  auto values = std::vector<uint32_t>({1000, 1001});
  UInt32ValueSet value_set;
  value_set.Add(absl::MakeSpan(values), 1);
  auto mock_get_key_value_set_result =
      std::make_unique<MockGetKeyValueSetResult>();
  EXPECT_CALL(*mock_get_key_value_set_result, GetUInt32ValueSet("key1"))
      .WillOnce(Return(&value_set));
  EXPECT_CALL(mock_cache_, GetUInt32ValueSet(_, _))
      .WillOnce(Return(std::move(mock_get_key_value_set_result)));
  auto local_lookup = CreateLocalLookup(mock_cache_);
  auto response =
      local_lookup->GetUInt32ValueSet(GetRequestContext(), {"key1"});
  ASSERT_TRUE(response.ok());
  EXPECT_THAT(response.value().kv_pairs().at("key1").uintset_values().values(),
              testing::UnorderedElementsAreArray(values));
}

TEST_F(LocalLookupTest, GetUInt32ValueSets_SetEmpty_Success) {
  auto mock_get_key_value_set_result =
      std::make_unique<MockGetKeyValueSetResult>();
  EXPECT_CALL(*mock_get_key_value_set_result, GetUInt32ValueSet("key1"))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(mock_cache_, GetUInt32ValueSet(_, _))
      .WillOnce(Return(std::move(mock_get_key_value_set_result)));
  auto local_lookup = CreateLocalLookup(mock_cache_);
  auto response =
      local_lookup->GetUInt32ValueSet(GetRequestContext(), {"key1"});
  ASSERT_TRUE(response.ok());
  InternalLookupResponse expected;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { status { code: 5 message: "Key not found: key1" } }
           }
      )pb",
      &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(LocalLookupTest, GetKeyValueSets_SetEmpty_Success) {
  auto mock_get_key_value_set_result =
      std::make_unique<MockGetKeyValueSetResult>();
  EXPECT_CALL(*mock_get_key_value_set_result, GetValueSet("key1"))
      .WillOnce(Return(absl::flat_hash_set<std::string_view>{}));
  EXPECT_CALL(mock_cache_, GetKeyValueSet(_, _))
      .WillOnce(Return(std::move(mock_get_key_value_set_result)));

  auto local_lookup = CreateLocalLookup(mock_cache_);
  auto response = local_lookup->GetKeyValueSet(GetRequestContext(), {"key1"});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { status { code: 5 message: "Key not found: key1" } }
           }
      )pb",
      &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(LocalLookupTest, GetKeyValueSet_EmptyRequest_ReturnsEmptyResponse) {
  auto local_lookup = CreateLocalLookup(mock_cache_);
  auto response = local_lookup->GetKeyValueSet(GetRequestContext(), {});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(LocalLookupTest, RunQuery_Success) {
  std::string query = "someset";

  auto mock_get_key_value_set_result =
      std::make_unique<MockGetKeyValueSetResult>();
  EXPECT_CALL(*mock_get_key_value_set_result, GetValueSet("someset"))
      .WillOnce(
          Return(absl::flat_hash_set<std::string_view>{"value1", "value2"}));
  EXPECT_CALL(
      mock_cache_,
      GetKeyValueSet(_, absl::flat_hash_set<std::string_view>{"someset"}))
      .WillOnce(Return(std::move(mock_get_key_value_set_result)));

  auto local_lookup = CreateLocalLookup(mock_cache_);
  auto response = local_lookup->RunQuery(GetRequestContext(), query);
  EXPECT_TRUE(response.ok());

  InternalRunQueryResponse expected;
  EXPECT_THAT(response.value().elements(),
              testing::UnorderedElementsAreArray({"value1", "value2"}));
}

TEST_F(LocalLookupTest, RunQuery_ParsingError_Error) {
  std::string query = "someset|(";

  auto local_lookup = CreateLocalLookup(mock_cache_);
  auto response = local_lookup->RunQuery(GetRequestContext(), query);
  EXPECT_FALSE(response.ok());
  EXPECT_EQ(response.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(LocalLookupTest, Verify_RunSetQueryInt_Success) {
  std::string query = "A";
  UInt32ValueSet value_set;
  auto values = std::vector<uint32_t>({10, 20, 30, 40, 50});
  value_set.Add(absl::MakeSpan(values), 1);
  auto mock_get_key_value_set_result =
      std::make_unique<MockGetKeyValueSetResult>();
  EXPECT_CALL(*mock_get_key_value_set_result, GetUInt32ValueSet("A"))
      .WillOnce(Return(&value_set));
  EXPECT_CALL(mock_cache_,
              GetUInt32ValueSet(_, absl::flat_hash_set<std::string_view>{"A"}))
      .WillOnce(Return(std::move(mock_get_key_value_set_result)));
  auto local_lookup = CreateLocalLookup(mock_cache_);
  auto response = local_lookup->RunSetQueryInt(GetRequestContext(), query);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response.value().elements(),
              testing::UnorderedElementsAreArray(values.begin(), values.end()));
}

TEST_F(LocalLookupTest, Verify_RunSetQueryInt_ParsingError_Error) {
  std::string query = "someset|(";
  auto local_lookup = CreateLocalLookup(mock_cache_);
  auto response = local_lookup->RunSetQueryInt(GetRequestContext(), query);
  EXPECT_FALSE(response.ok());
  EXPECT_EQ(response.status().code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace

}  // namespace kv_server
