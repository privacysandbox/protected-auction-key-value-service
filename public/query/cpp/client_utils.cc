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

#include "public/query/cpp/client_utils.h"

#include "google/protobuf/util/json_util.h"
#include "src/cpp/util/status_macro/status_macros.h"

namespace kv_server {

using google::protobuf::util::MessageToJsonString;

absl::StatusOr<std::string> ToJson(const v2::GetValuesRequest& request_proto) {
  std::string json;
  PS_RETURN_IF_ERROR(MessageToJsonString(request_proto, &json));
  return json;
}

}  // namespace kv_server
