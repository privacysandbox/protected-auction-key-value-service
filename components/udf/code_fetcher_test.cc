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
#include "components/udf/code_fetcher.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "components/internal_lookup/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "roma/interface/roma.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using testing::_;
using testing::Return;

TEST(CodeFetcherTest, CreatesCodeConfigWithCodeSnippetAndHandlerName) {
  MockLookupClient mock_lookup_client;

  std::vector<std::string> udf_config_keys = {"udf_handler_name",
                                              "udf_code_snippet"};
  InternalLookupResponse response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "udf_handler_name"
                                     value { value: "SomeHandler" }
                                   }
                                   kv_pairs {
                                     key: "udf_code_snippet"
                                     value { value: "function SomeUDFCode(){}" }
                                   })pb",
                              &response);
  EXPECT_CALL(mock_lookup_client, GetValues(udf_config_keys))
      .WillOnce(Return(response));
  auto code_fetcher = CodeFetcher::Create();
  auto code_config = code_fetcher->FetchCodeConfig(mock_lookup_client);
  EXPECT_TRUE(code_config.ok());
  EXPECT_EQ(code_config->js, "function SomeUDFCode(){}");
  EXPECT_EQ(code_config->udf_handler_name, "SomeHandler");
}

TEST(CodeFetcherTest, ErrorOnEmptyUdfCodeSnippet) {
  MockLookupClient mock_lookup_client;

  std::vector<std::string> udf_config_keys = {"udf_handler_name",
                                              "udf_code_snippet"};
  InternalLookupResponse response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "udf_handler_name"
                                     value { value: "Something" }
                                   }
                                   kv_pairs {
                                     key: "udf_code_snippet"
                                     value { value: "" }
                                   })pb",
                              &response);
  EXPECT_CALL(mock_lookup_client, GetValues(udf_config_keys))
      .WillOnce(Return(response));
  auto code_fetcher = CodeFetcher::Create();
  auto code_config = code_fetcher->FetchCodeConfig(mock_lookup_client);
  EXPECT_EQ(code_config.status(),
            absl::NotFoundError("UDF code snippet value empty"));
}

TEST(CodeFetcherTest, ErrorOnEmptyUdfHandlerNameSnippet) {
  MockLookupClient mock_lookup_client;

  std::vector<std::string> udf_config_keys = {"udf_handler_name",
                                              "udf_code_snippet"};
  InternalLookupResponse response;
  TextFormat::ParseFromString(R"pb(kv_pairs {
                                     key: "udf_handler_name"
                                     value { value: "" }
                                   }
                                   kv_pairs {
                                     key: "udf_code_snippet"
                                     value { value: "Something" }
                                   })pb",
                              &response);
  EXPECT_CALL(mock_lookup_client, GetValues(udf_config_keys))
      .WillOnce(Return(response));

  auto code_fetcher = CodeFetcher::Create();
  auto code_config = code_fetcher->FetchCodeConfig(mock_lookup_client);
  EXPECT_EQ(code_config.status(),
            absl::NotFoundError("UDF handler name value empty"));
}

TEST(CodeFetcherTest, ErrorOnUdfHandlerNameWithErrorStatus) {
  MockLookupClient mock_lookup_client;

  std::vector<std::string> udf_config_keys = {"udf_handler_name",
                                              "udf_code_snippet"};
  InternalLookupResponse response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "udf_handler_name"
             value { status { code: 2 message: "Some error" } }
           }
           kv_pairs {
             key: "udf_code_snippet"
             value { value: "function SomeUDFCode(){}" }
           })pb",
      &response);
  EXPECT_CALL(mock_lookup_client, GetValues(udf_config_keys))
      .WillOnce(Return(response));

  auto code_fetcher = CodeFetcher::Create();
  auto code_config = code_fetcher->FetchCodeConfig(mock_lookup_client);
  EXPECT_EQ(code_config.status(), absl::UnknownError("Some error"));
}

TEST(CodeFetcherTest, ErrorOnUdfCodeSnippetWithErrorStatus) {
  MockLookupClient mock_lookup_client;

  std::vector<std::string> udf_config_keys = {"udf_handler_name",
                                              "udf_code_snippet"};
  InternalLookupResponse response;
  TextFormat::ParseFromString(
      R"pb(kv_pairs {
             key: "udf_handler_name"
             value { value: "Something" }
           }
           kv_pairs {
             key: "udf_code_snippet"
             value { status { code: 2 message: "Some error" } }
           })pb",
      &response);
  EXPECT_CALL(mock_lookup_client, GetValues(udf_config_keys))
      .WillOnce(Return(response));

  auto code_fetcher = CodeFetcher::Create();
  auto code_config = code_fetcher->FetchCodeConfig(mock_lookup_client);
  EXPECT_EQ(code_config.status(), absl::UnknownError("Some error"));
}

TEST(CodeFetcherTest, ErrorOnLookupError) {
  MockLookupClient mock_lookup_client;

  std::vector<std::string> udf_config_keys = {"udf_handler_name",
                                              "udf_code_snippet"};
  EXPECT_CALL(mock_lookup_client, GetValues(udf_config_keys))
      .WillOnce(Return(absl::UnknownError("Some error")));

  auto code_fetcher = CodeFetcher::Create();
  auto code_config = code_fetcher->FetchCodeConfig(mock_lookup_client);
  EXPECT_EQ(code_config.status(), absl::UnknownError("Some error"));
}

TEST(CodeFetcherTest, ErrorOnNoUdfHandlerName) {
  MockLookupClient mock_lookup_client;

  std::vector<std::string> udf_config_keys = {"udf_handler_name",
                                              "udf_code_snippet"};
  InternalLookupResponse response;
  TextFormat::ParseFromString(
      R"pb(
        kv_pairs {
          key: "udf_code_snippet"
          value { value: "Something" }
        })pb",
      &response);
  EXPECT_CALL(mock_lookup_client, GetValues(udf_config_keys))
      .WillOnce(Return(response));

  auto code_fetcher = CodeFetcher::Create();
  auto code_config = code_fetcher->FetchCodeConfig(mock_lookup_client);
  EXPECT_EQ(code_config.status(),
            absl::NotFoundError("UDF handler name not found"));
}

}  // namespace
}  // namespace kv_server
