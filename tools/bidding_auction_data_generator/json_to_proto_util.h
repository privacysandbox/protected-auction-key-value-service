/*
 * Copyright 2025 Google LLC
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

#ifndef TOOLS_BIDDING_AUCTION_DATA_GENERATOR_JSON_TO_PROTO_UTIL_H_
#define TOOLS_BIDDING_AUCTION_DATA_GENERATOR_JSON_TO_PROTO_UTIL_H_

#include <algorithm>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "tools/bidding_auction_data_generator/data/custom_audience_data.pb.h"

namespace kv_server {

// Callback function to extract buyer key values under "keys" namespace
// from GetValueResponse and put them in the map
std::function<void(
    kv_server::tools::bidding_auction_data_generator::BYOSGetValuesResponse&
        response,
    absl::flat_hash_map<std::string, std::string>& key_value_map)>
GetDataExtractorForKeys();

// Callback function to extract seller key values under "renderUrls" namespace
// from GetValueResponse and put them in the map
std::function<void(
    kv_server::tools::bidding_auction_data_generator::BYOSGetValuesResponse&
        response,
    absl::flat_hash_map<std::string, std::string>& key_value_map)>
GetDataExtractorForRenderUrls();

// Callback function to extract seller key values under "adComponentRenderUrls"
// namespace from GetValueResponse and put them in the map
std::function<void(
    kv_server::tools::bidding_auction_data_generator::BYOSGetValuesResponse&
        response,
    absl::flat_hash_map<std::string, std::string>& key_value_map)>
GetDataExtractorForAdComponentRenderUrls();

absl::StatusOr<
    kv_server::tools::bidding_auction_data_generator::BYOSGetValuesResponse>
ReadGetValueResponseJsonFile(const std::string& file_path);

absl::StatusOr<absl::flat_hash_set<std::string>> GetInputJsonFilePaths(
    const std::string& input_file_dir);

bool IsJsonFile(const std::string& file_path);

}  // namespace kv_server

#endif  // TOOLS_BIDDING_AUCTION_DATA_GENERATOR_JSON_TO_PROTO_UTIL_H_
