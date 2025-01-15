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

#ifndef TOOLS_BIDDING_AUCTION_DATA_GENERATOR_HTTP_VALUE_RETRIEVER_H_
#define TOOLS_BIDDING_AUCTION_DATA_GENERATOR_HTTP_VALUE_RETRIEVER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "tools/bidding_auction_data_generator/data/custom_audience_data.pb.h"
#include "tools/bidding_auction_data_generator/http_url_fetch_client.h"

namespace kv_server {
// Defines the class that retrieves values from KV server for buyer keys and
// seller keys
class HttpValueRetriever {
 public:
  static std::unique_ptr<HttpValueRetriever> Create(
      HttpUrlFetchClient& http_url_fetch_client);
  explicit HttpValueRetriever(HttpUrlFetchClient& http_url_fetch_client)
      : http_url_fetch_client_(http_url_fetch_client) {}
  virtual ~HttpValueRetriever() = default;

  // Breaks the keys into different batches and generate url for each batch
  // request
  static std::vector<std::string> GetBatchedUrls(
      const absl::flat_hash_set<std::string>& keys,
      const std::string& key_namespace, const std::string& base_url,
      bool need_encode, int num_keys_in_batch);
  // Sends requests and parses the key value pairs from responses and returns
  // the output, which is a vector of key value maps.
  absl::StatusOr<std::vector<absl::flat_hash_map<std::string, std::string>>>
  RetrieveValues(
      const absl::flat_hash_set<std::string>& keys, const std::string& base_url,
      const std::string& key_namespace,
      std::function<void(kv_server::tools::bidding_auction_data_generator::
                             BYOSGetValuesResponse&,
                         absl::flat_hash_map<std::string, std::string>&)>
          callback,
      bool need_encode, int num_keys_per_batch);

 private:
  HttpUrlFetchClient& http_url_fetch_client_;
};

}  // namespace kv_server

#endif  // TOOLS_BIDDING_AUCTION_DATA_GENERATOR_HTTP_VALUE_RETRIEVER_H_
