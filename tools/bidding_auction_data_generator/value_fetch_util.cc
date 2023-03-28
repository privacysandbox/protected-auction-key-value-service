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

#include "tools/bidding_auction_data_generator/value_fetch_util.h"

#include <algorithm>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "curl/curl.h"
#include "google/protobuf/util/json_util.h"
#include "public/query/get_values.pb.h"

namespace kv_server {
namespace {
std::string EncodeParam(std::string param) {
  char* encoded_chars = curl_easy_escape(NULL, param.c_str(), param.length());
  std::string encoded_string(encoded_chars);
  curl_free(encoded_chars);
  return encoded_string;
}

bool IsEmptyValue(const google::protobuf::Value& value) {
  return (value.has_list_value() && value.list_value().values_size() == 0) ||
         (value.has_struct_value() &&
          value.struct_value().fields_size() == 0) ||
         (value.has_string_value() && value.string_value().empty());
}
}  // namespace

std::string AppendQueryParametersToUrl(const std::string& base_url,
                                       const std::string& key,
                                       const std::vector<std::string>& params,
                                       bool need_encode) {
  std::string url(base_url);
  absl::StrAppend(&url, key, "=");
  if (need_encode) {
    std::vector<std::string> encoded_params(params.size());
    std::transform(params.cbegin(), params.cend(), encoded_params.begin(),
                   [](const std::string& param) { return EncodeParam(param); });
    absl::StrAppend(&url, absl::StrJoin(encoded_params, ","));
  } else {
    absl::StrAppend(&url, absl::StrJoin(params, ","));
  }
  return url;
}

std::function<
    void(v1::GetValuesResponse& response,
         absl::flat_hash_map<std::string, std::string>& key_value_map)>
GetDataExtractorForKeys() {
  return [](v1::GetValuesResponse& response,
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
    void(v1::GetValuesResponse& response,
         absl::flat_hash_map<std::string, std::string>& key_value_map)>
GetDataExtractorForRenderUrls() {
  return [](v1::GetValuesResponse& response,
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

}  // namespace kv_server
