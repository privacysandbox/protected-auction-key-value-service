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
#include "tools/bidding_auction_data_generator/custom_audience_data_parser.h"

#include "google/protobuf/util/json_util.h"
#include "tools/bidding_auction_data_generator/data/custom_audience_data.pb.h"

namespace kv_server {
using SideLoadData =
    ::kv_server::tools::bidding_auction_data_generator::SideLoadData;
absl::Status ParseAudienceData(
    const std::string& side_load_json,
    absl::flat_hash_set<std::string>& custom_audience_names,
    absl::flat_hash_set<std::string>& render_urls) {
  SideLoadData side_load_proto_data;
  google::protobuf::util::Status result =
      google::protobuf::util::JsonStringToMessage(side_load_json,
                                                  &side_load_proto_data);
  if (!result.ok()) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Failed to convert json to side load proto data:", result.ToString()));
  }

  // Parse custom audience names
  for (auto& ca : side_load_proto_data.interest_groups()) {
    custom_audience_names.insert(ca.name());
    // Parse render urls
    for (auto& ad : ca.ads()) {
      render_urls.insert(ad.render_url());
    }
  }
  return absl::OkStatus();
}
}  // namespace kv_server
