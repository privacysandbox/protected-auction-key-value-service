
/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tools/bidding_auction_data_generator/http_value_retriever.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "tools/bidding_auction_data_generator/mocks.h"

namespace kv_server {
namespace {
using Output = std::vector<absl::flat_hash_map<std::string, std::string>>;

TEST(HttpUrlFetchClientTest, UrlsFetchTest) {
  HttpUrlFetchClient testClient;
  std::vector<std::string> test_urls = {"www.google.com", "www.wikipedia.com"};
  absl::StatusOr<std::vector<std::string>> status =
      testClient.FetchUrls(test_urls, 2000);
  EXPECT_TRUE(status.ok());
  EXPECT_THAT(status.value().size(), test_urls.size());
}

TEST(HttpUrlFetchClientTest, BuyerUrlsFetchTest) {
  kv_server::MockHttpUrlFetchClient mock_http_url_fetch_client;
  const std::string json_response(R"(
  {
    "keys":{
      "key1":["v1"],
      "key2":["v2"],
    }
  })");
  std::vector<std::string> responses;
  responses.push_back(json_response);
  ON_CALL(mock_http_url_fetch_client, FetchUrls)
      .WillByDefault(
          [&responses](
              const std::vector<std::string>& urls,
              int64_t timeout_ms) -> absl::StatusOr<std::vector<std::string>> {
            return responses;
          });

  EXPECT_CALL(mock_http_url_fetch_client, FetchUrls).Times(1);

  std::unique_ptr<kv_server::HttpValueRetriever> test_value_retriever =
      HttpValueRetriever::Create(mock_http_url_fetch_client);
  absl::flat_hash_set<std::string> keys = {"key1", "key2"};
  absl::StatusOr<Output> buyer_output = test_value_retriever->RetrieveValues(
      keys, "https:://test_domain?", "keys",
      kv_server::GetDataExtractorForKeys(), false, 50);
  EXPECT_TRUE(buyer_output.ok());
  EXPECT_THAT(buyer_output.value().size(), 1);
  EXPECT_THAT(buyer_output.value()[0].find("key1")->second, R"(["v1"])");
  EXPECT_THAT(buyer_output.value()[0].find("key2")->second, R"(["v2"])");
}

TEST(HttpUrlFetchClientTest, BuyerSkipEmptyValueTest) {
  kv_server::MockHttpUrlFetchClient mock_http_url_fetch_client;
  const std::string json_response(R"(
  {
    "keys":{
      "key1":[],
      "key2":{},
      "key3":["v3"]
    }
  })");
  std::vector<std::string> responses;
  responses.push_back(json_response);
  ON_CALL(mock_http_url_fetch_client, FetchUrls)
      .WillByDefault(
          [&responses](
              const std::vector<std::string>& urls,
              int64_t timeout_ms) -> absl::StatusOr<std::vector<std::string>> {
            return responses;
          });

  EXPECT_CALL(mock_http_url_fetch_client, FetchUrls).Times(1);

  std::unique_ptr<kv_server::HttpValueRetriever> test_value_retriever =
      HttpValueRetriever::Create(mock_http_url_fetch_client);
  absl::flat_hash_set<std::string> keys = {"key1", "key2", "key3"};
  absl::StatusOr<Output> buyer_output = test_value_retriever->RetrieveValues(
      keys, "https:://test_domain?", "keys",
      kv_server::GetDataExtractorForKeys(), false, 50);
  EXPECT_TRUE(buyer_output.ok());
  EXPECT_THAT(buyer_output.value().size(), 1);
  EXPECT_THAT(buyer_output.value()[0].size(), 1);
  EXPECT_THAT(buyer_output.value()[0].find("key3")->second, R"(["v3"])");
}

TEST(HttpUrlFetchClientTest, SellerUrlsFetchTest) {
  kv_server::MockHttpUrlFetchClient mock_http_url_fetch_client;
  const std::string json_response(R"(
  {
    "renderUrls":{
      "url1":["v1"],
      "url2":["v2"],
    }
  })");
  std::vector<std::string> responses;
  responses.push_back(json_response);
  ON_CALL(mock_http_url_fetch_client, FetchUrls)
      .WillByDefault(
          [&responses](
              const std::vector<std::string>& urls,
              int64_t timeout_ms) -> absl::StatusOr<std::vector<std::string>> {
            return responses;
          });

  std::unique_ptr<kv_server::HttpValueRetriever> test_value_retriever =
      HttpValueRetriever::Create(mock_http_url_fetch_client);
  absl::flat_hash_set<std::string> renderUrls = {"url1", "url2"};
  absl::StatusOr<Output> seller_output = test_value_retriever->RetrieveValues(
      renderUrls, "https:://test_domain?", "renderUrls",
      kv_server::GetDataExtractorForRenderUrls(), true, 50);
  EXPECT_TRUE(seller_output.ok());
  EXPECT_THAT(seller_output.value().size(), 1);
  EXPECT_THAT(seller_output.value()[0].find("url1")->second, R"(["v1"])");
  EXPECT_THAT(seller_output.value()[0].find("url2")->second, R"(["v2"])");
}

TEST(HttpUrlFetchClientTest, SellerSkipEmptyValueTest) {
  kv_server::MockHttpUrlFetchClient mock_http_url_fetch_client;
  const std::string json_response(R"(
  {
    "renderUrls":{
      "url1":[],
      "url2":{},
      "url3":["v3"]
    }
  })");
  std::vector<std::string> responses;
  responses.push_back(json_response);
  ON_CALL(mock_http_url_fetch_client, FetchUrls)
      .WillByDefault(
          [&responses](
              const std::vector<std::string>& urls,
              int64_t timeout_ms) -> absl::StatusOr<std::vector<std::string>> {
            return responses;
          });

  EXPECT_CALL(mock_http_url_fetch_client, FetchUrls).Times(1);

  std::unique_ptr<kv_server::HttpValueRetriever> test_value_retriever =
      HttpValueRetriever::Create(mock_http_url_fetch_client);
  absl::flat_hash_set<std::string> renderUrls = {"url1", "url2", "url3"};
  absl::StatusOr<Output> seller_output = test_value_retriever->RetrieveValues(
      renderUrls, "https:://test_domain?", "renderUrls",
      kv_server::GetDataExtractorForRenderUrls(), true, 50);
  EXPECT_TRUE(seller_output.ok());
  EXPECT_THAT(seller_output.value().size(), 1);
  EXPECT_THAT(seller_output.value()[0].size(), 1);
  EXPECT_THAT(seller_output.value()[0].find("url3")->second, R"(["v3"])");
}

}  // namespace
}  // namespace kv_server
