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

#include "public/applications/pa/response_utils.h"

#include "components/errors/error_tag.h"
#include "google/protobuf/util/json_util.h"
#include "src/util/status_macro/status_macros.h"

namespace kv_server::application_pa {

enum class ErrorTag : int {
  kJsonStringToMessageError = 1,
  kMessageToJsonStringError = 2
};

using google::protobuf::util::JsonStringToMessage;
using google::protobuf::util::MessageToJsonString;

absl::StatusOr<KeyGroupOutputs> KeyGroupOutputsFromJson(
    std::string_view json_str) {
  KeyGroupOutputs outputs_proto;
  if (const auto status = JsonStringToMessage(json_str, &outputs_proto);
      !status.ok()) {
    return StatusWithErrorTag(status, __FILE__,
                              ErrorTag::kJsonStringToMessageError);
  }
  return outputs_proto;
}

absl::StatusOr<std::string> KeyGroupOutputsToJson(
    const KeyGroupOutputs& key_group_outputs) {
  std::string json_str;
  if (const auto status = MessageToJsonString(key_group_outputs, &json_str);
      !status.ok()) {
    return StatusWithErrorTag(status, __FILE__,
                              ErrorTag::kMessageToJsonStringError);
  }
  return json_str;
}

}  // namespace kv_server::application_pa
