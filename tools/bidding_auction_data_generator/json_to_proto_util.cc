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

#include "tools/bidding_auction_data_generator/json_to_proto_util.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "curl/curl.h"
#include "google/protobuf/util/json_util.h"

namespace kv_server {
namespace {

using BYOSGetValuesResponse =
    kv_server::tools::bidding_auction_data_generator::BYOSGetValuesResponse;
constexpr std::string_view kJsonFileExtention = "json";

bool IsEmptyValue(const google::protobuf::Value& value) {
  return (value.has_list_value() && value.list_value().values_size() == 0) ||
         (value.has_struct_value() &&
          value.struct_value().fields_size() == 0) ||
         (value.has_string_value() && value.string_value().empty());
}
}  // namespace

std::function<
    void(BYOSGetValuesResponse& response,
         absl::flat_hash_map<std::string, std::string>& key_value_map)>
GetDataExtractorForKeys() {
  return [](BYOSGetValuesResponse& response,
            absl::flat_hash_map<std::string, std::string>& key_value_map) {
    for (auto& [k, v] : response.keys().fields()) {
      if (!IsEmptyValue(v)) {
        std::string value;
        google::protobuf::util::MessageToJsonString(v, &value);
        key_value_map.emplace(k, value);
      }
    }
  };
}

std::function<
    void(BYOSGetValuesResponse& response,
         absl::flat_hash_map<std::string, std::string>& key_value_map)>
GetDataExtractorForRenderUrls() {
  return [](BYOSGetValuesResponse& response,
            absl::flat_hash_map<std::string, std::string>& key_value_map) {
    for (auto& [k, v] : response.render_urls().fields()) {
      if (!IsEmptyValue(v)) {
        std::string value;
        google::protobuf::util::MessageToJsonString(v, &value);
        key_value_map.emplace(k, value);
      }
    }
  };
}

std::function<
    void(BYOSGetValuesResponse& response,
         absl::flat_hash_map<std::string, std::string>& key_value_map)>
GetDataExtractorForAdComponentRenderUrls() {
  return [](BYOSGetValuesResponse& response,
            absl::flat_hash_map<std::string, std::string>& key_value_map) {
    for (auto& [k, v] : response.ad_component_render_urls().fields()) {
      if (!IsEmptyValue(v)) {
        std::string value;
        google::protobuf::util::MessageToJsonString(v, &value);
        key_value_map.emplace(k, value);
      }
    }
  };
}

absl::StatusOr<BYOSGetValuesResponse> ReadGetValueResponseJsonFile(
    const std::string& file_path) {
  std::ifstream in_stream(file_path);
  LOG(INFO) << "Reading " << file_path << " to get BYOS GetValuesResponse data";
  if (in_stream) {
    google::protobuf::util::JsonParseOptions json_parse_options;
    json_parse_options.ignore_unknown_fields = true;
    std::ostringstream contents;
    contents << in_stream.rdbuf();
    in_stream.close();
    BYOSGetValuesResponse response;
    absl::Status result = google::protobuf::util::JsonStringToMessage(
        contents.str(), &response, json_parse_options);
    if (!result.ok()) {
      return absl::FailedPreconditionError(
          absl::StrCat("Failed to convert json to BYOS GetValuesResponse data:",
                       result.ToString()));
    }
    return std::move(response);
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Unable to read the file:", file_path));
}

absl::StatusOr<absl::flat_hash_set<std::string>> GetInputJsonFilePaths(
    const std::string& input_file_dir) {
  absl::flat_hash_set<std::string> result;
  if (input_file_dir.empty()) {
    return result;
  }
  std::error_code error_code;
  std::filesystem::directory_iterator it(input_file_dir, error_code);
  if (error_code) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Error creating directory_iterator: ", error_code.message()));
  }
  for (auto const& file : it) {
    const std::string file_name =
        std::filesystem::path(file).filename().string();
    if (IsJsonFile(file_name)) {
      result.emplace(absl::StrCat(input_file_dir, "/", file_name));
    }
  }
  return result;
}

bool IsJsonFile(const std::string& file_path) {
  std::string extension = file_path.substr(file_path.find_last_of(".") + 1);
  return kJsonFileExtention == absl::AsciiStrToLower(extension);
}

}  // namespace kv_server
