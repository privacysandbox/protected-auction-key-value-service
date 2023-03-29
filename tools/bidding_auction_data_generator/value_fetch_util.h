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

#ifndef TOOLS_BIDDING_AUCTION_DATA_GENERATOR_VALUE_FETCH_UTIL_H_
#define TOOLS_BIDDING_AUCTION_DATA_GENERATOR_VALUE_FETCH_UTIL_H_

#include <algorithm>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "public/query/get_values.pb.h"

namespace kv_server {

// Appends query parameters to the url
std::string AppendQueryParametersToUrl(const std::string& base_url,
                                       const std::string& key,
                                       const std::vector<std::string>& params,
                                       bool need_encode);

// Callback function to extract buyer key values under "keys" namespace
// from GetValueResponse and put them in the map
std::function<
    void(v1::GetValuesResponse& response,
         absl::flat_hash_map<std::string, std::string>& key_value_map)>
GetDataExtractorForKeys();

// Callback function to extract seller key values under "renderUrls" namespace
// from GetValueResponse and put them in the map
std::function<
    void(v1::GetValuesResponse& response,
         absl::flat_hash_map<std::string, std::string>& key_value_map)>
GetDataExtractorForRenderUrls();

}  // namespace kv_server

#endif  // TOOLS_BIDDING_AUCTION_DATA_GENERATOR_VALUE_FETCH_UTIL_H_
