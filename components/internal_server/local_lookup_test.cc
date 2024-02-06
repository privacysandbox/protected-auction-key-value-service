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
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using privacy_sandbox::server_common::MockMetricsRecorder;
using testing::_;
using testing::Return;
using testing::ReturnRef;

class LocalLookupTest : public ::testing::Test {
 protected:
  LocalLookupTest() {
    InitMetricsContextMap();
    scope_metrics_context_ = std::make_unique<ScopeMetricsContext>();
    request_context_ =
        std::make_unique<RequestContext>(*scope_metrics_context_);
  }
  RequestContext& GetRequestContext() { return *request_context_; }
  std::unique_ptr<ScopeMetricsContext> scope_metrics_context_;
  std::unique_ptr<RequestContext> request_context_;
  MockCache mock_cache_;
  MockMetricsRecorder mock_metrics_recorder_;
};

TEST_F(LocalLookupTest, GetKeyValues_KeysFound_Success) {
  EXPECT_CALL(mock_cache_, GetKeyValuePairs(_, _))
      .WillOnce(Return(absl::flat_hash_map<std::string, std::string>{
          {"key1", "value1"}, {"key2", "value2"}}));

  auto local_lookup = CreateLocalLookup(mock_cache_, mock_metrics_recorder_);
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

  auto local_lookup = CreateLocalLookup(mock_cache_, mock_metrics_recorder_);
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

  auto local_lookup = CreateLocalLookup(mock_cache_, mock_metrics_recorder_);
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
             value { status { code: 5 message: "Key not found" } }
           }
      )pb",
      &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(LocalLookupTest, GetKeyValues_EmptyRequest_ReturnsEmptyResponse) {
  auto local_lookup = CreateLocalLookup(mock_cache_, mock_metrics_recorder_);
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

  auto local_lookup = CreateLocalLookup(mock_cache_, mock_metrics_recorder_);
  auto response = local_lookup->GetKeyValueSet(GetRequestContext(), {"key1"});
  EXPECT_TRUE(response.ok());

  std::vector<std::string> expected_resulting_set = {"value1", "value2"};
  EXPECT_THAT(response.value().kv_pairs().at("key1").keyset_values().values(),
              testing::UnorderedElementsAreArray(expected_resulting_set));
}

TEST_F(LocalLookupTest, GetKeyValueSets_SetEmpty_Success) {
  auto mock_get_key_value_set_result =
      std::make_unique<MockGetKeyValueSetResult>();
  EXPECT_CALL(*mock_get_key_value_set_result, GetValueSet("key1"))
      .WillOnce(Return(absl::flat_hash_set<std::string_view>{}));
  EXPECT_CALL(mock_cache_, GetKeyValueSet(_, _))
      .WillOnce(Return(std::move(mock_get_key_value_set_result)));

  auto local_lookup = CreateLocalLookup(mock_cache_, mock_metrics_recorder_);
  auto response = local_lookup->GetKeyValueSet(GetRequestContext(), {"key1"});
  EXPECT_TRUE(response.ok());

  InternalLookupResponse expected;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "key1"
             value { status { code: 5 message: "Key not found" } }
           }
      )pb",
      &expected);
  EXPECT_THAT(response.value(), EqualsProto(expected));
}

TEST_F(LocalLookupTest, GetKeyValueSet_EmptyRequest_ReturnsEmptyResponse) {
  auto local_lookup = CreateLocalLookup(mock_cache_, mock_metrics_recorder_);
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

  auto local_lookup = CreateLocalLookup(mock_cache_, mock_metrics_recorder_);
  auto response = local_lookup->RunQuery(GetRequestContext(), query);
  EXPECT_TRUE(response.ok());

  InternalRunQueryResponse expected;
  EXPECT_THAT(response.value().elements(),
              testing::UnorderedElementsAreArray({"value1", "value2"}));
}

TEST_F(LocalLookupTest, RunQuery_ParsingError_Error) {
  std::string query = "someset|(";

  auto local_lookup = CreateLocalLookup(mock_cache_, mock_metrics_recorder_);
  auto response = local_lookup->RunQuery(GetRequestContext(), query);
  EXPECT_FALSE(response.ok());
  EXPECT_EQ(response.status().code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace

}  // namespace kv_server
