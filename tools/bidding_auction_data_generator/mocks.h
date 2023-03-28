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

#ifndef TOOLS_BIDDING_AUCTION_DATA_GENERATOR_MOCKS_H_
#define TOOLS_BIDDING_AUCTION_DATA_GENERATOR_MOCKS_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "tools/bidding_auction_data_generator/http_url_fetch_client.h"

namespace kv_server {

class MockHttpUrlFetchClient : public HttpUrlFetchClient {
 public:
  MOCK_METHOD(absl::StatusOr<std::vector<std::string>>, FetchUrls,
              (const std::vector<std::string>& urls, int64_t timeout_ms));
};
}  // namespace kv_server

#endif  // TOOLS_BIDDING_AUCTION_DATA_GENERATOR_MOCKS_H_
