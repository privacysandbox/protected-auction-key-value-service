// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tools/request_simulation/request_generation_util.h"

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "tools/request_simulation/request/raw_request.pb.h"

namespace kv_server {

// TODO(b/289240702): Make the request json schema easily configurable from
// external configure file

constexpr std::string_view kKVV2KeyValueDSPRequestBodyFormat = R"json(
{"metadata": {}, "log_context": {"generation_id": "%s", "adtech_debug_id": "debug_id"}, "consented_debug_config": {"is_consented": true, "token": "%s"}, "partitions": [{ "id": 0, "compressionGroupId": 0,"arguments": [{ "tags": [ "custom", "keys" ],"data": [ %s ] }] }] })json";

std::vector<std::string> GenerateRandomKeys(int number_of_keys, int key_size) {
  std::vector<std::string> result;
  for (int i = 0; i < number_of_keys; ++i) {
    result.push_back(std::string(key_size, 'A' + (std::rand() % 26)));
  }
  return result;
}

std::string CreateKVDSPRequestBodyInJson(
    const std::vector<std::string>& keys,
    std::string_view consented_debug_token,
    std::optional<std::string> generation_id_override) {
  const std::string comma_seperated_keys =
      absl::StrJoin(keys, ",", [](std::string* out, const std::string& key) {
        absl::StrAppend(out, "\"", key, "\"");
      });
  const std::string generation_id = generation_id_override.has_value()
                                        ? generation_id_override.value()
                                        : std::to_string(std::rand());
  return absl::StrFormat(kKVV2KeyValueDSPRequestBodyFormat, generation_id,
                         std::string(consented_debug_token),
                         comma_seperated_keys);
}

// TODO(b/289240702): Explore if there is a way to create dynamic Message
// directly from request body in json and protobuf descriptor. So that request
// schema does not have to be tied to a specific proto file, user can only need
// to pass the protoset binary file and the system automatically generate
// corresponding Message based on the message name and constructed descriptor
// pool. Currently it is not easy because
// google::protobuf::util::JsonStringToMessage does not work well with message
// dependent on google.api.HttpBody
kv_server::RawRequest CreatePlainTextRequest(
    const std::string& request_in_json) {
  kv_server::RawRequest request;
  request.mutable_raw_body()->set_data(request_in_json);
  return request;
}

}  // namespace kv_server
