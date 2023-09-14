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

#include <fstream>
#include <sstream>

#include "google/protobuf/util/json_util.h"
#include "tools/bidding_auction_data_generator/data/custom_audience_data.pb.h"

namespace kv_server {
using SideLoadData =
    kv_server::tools::bidding_auction_data_generator::SideLoadData;

absl::StatusOr<SideLoadData> ReadCustomAudienceData(
    const std::string& file_path) {
  std::ifstream in_stream(file_path);
  if (in_stream) {
    std::ostringstream contents;
    contents << in_stream.rdbuf();
    in_stream.close();
    SideLoadData side_load_proto_data;
    absl::Status result = google::protobuf::util::JsonStringToMessage(
        contents.str(), &side_load_proto_data);
    if (!result.ok()) {
      return absl::FailedPreconditionError(
          absl::StrCat("Failed to convert json to side load proto data:",
                       result.ToString()));
    }
    return side_load_proto_data;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Unable to read the file,", file_path));
}

void ParseAudienceData(const SideLoadData& custom_audience_data,
                       absl::flat_hash_set<std::string>& custom_audience_names,
                       absl::flat_hash_set<std::string>& render_urls) {
  // Parse custom audience names
  for (auto& ca : custom_audience_data.interest_groups()) {
    custom_audience_names.insert(ca.name());
    // Parse render urls
    for (auto& ad : ca.ads()) {
      render_urls.insert(ad.render_url());
    }
  }
}
}  // namespace kv_server
