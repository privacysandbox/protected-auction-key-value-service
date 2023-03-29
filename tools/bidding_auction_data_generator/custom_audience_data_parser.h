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

#ifndef TOOLS_BIDDING_AUCTION_DATA_GENERATOR_CUSTOM_AUDIENCE_DATA_PARSER_H_
#define TOOLS_BIDDING_AUCTION_DATA_GENERATOR_CUSTOM_AUDIENCE_DATA_PARSER_H_

#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "tools/bidding_auction_data_generator/data/custom_audience_data.pb.h"

namespace kv_server {

// Reads custom audience data from json file
absl::StatusOr<kv_server::tools::bidding_auction_data_generator::SideLoadData>
ReadCustomAudienceData(const std::string& file_path);

// Defines a function that parses custom audience names and render_urls from
// custom audience data proto message (tools/data/custom_audience_data.proto).
// Custom audience names can be buyer keys to test against DSP KV server and
// render_urls can be seller keys to test against SSP KV server.
void ParseAudienceData(
    const kv_server::tools::bidding_auction_data_generator::SideLoadData&
        custom_audience_data,
    absl::flat_hash_set<std::string>& custom_audience_names,
    absl::flat_hash_set<std::string>& render_urls);

}  // namespace kv_server

#endif  // TOOLS_BIDDING_AUCTION_DATA_GENERATOR_CUSTOM_AUDIENCE_DATA_PARSER_H_
