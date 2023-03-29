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

#ifndef TOOLS_BIDDING_AUCTION_DATA_GENERATOR_HTTP_URL_FETCH_CLIENT_H_
#define TOOLS_BIDDING_AUCTION_DATA_GENERATOR_HTTP_URL_FETCH_CLIENT_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "curl/curl.h"

namespace kv_server {
// Defines a class that sends http requests and fetches the responses
class HttpUrlFetchClient {
 public:
  HttpUrlFetchClient() { curl_global_init(CURL_GLOBAL_ALL); }
  virtual ~HttpUrlFetchClient() { curl_global_cleanup(); }
  // Sends http requests for the given urls and saves the response to the
  // response vector
  virtual absl::StatusOr<std::vector<std::string>> FetchUrls(
      const std::vector<std::string>& urls, int64_t timeout_ms);
};

}  // namespace kv_server

#endif  // TOOLS_BIDDING_AUCTION_DATA_GENERATOR_HTTP_URL_FETCH_CLIENT_H_
