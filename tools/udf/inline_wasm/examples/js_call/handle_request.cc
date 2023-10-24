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

#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "emscripten/bind.h"
#include "nlohmann/json.hpp"

namespace {

// Calls getValues for a list of keys and processes the response.
absl::StatusOr<emscripten::val> getKvPairs(const emscripten::val& get_values_cb,
                                           const emscripten::val& keys) {
  // `get_values_cb` takes an emscripten::val of type array and returns
  // an emscripten::val of type string.
  // We need to cast the emscripten::val string to a C++ string.
  // However, getValues actually returns a serialized JSON object,
  // so we need to parse the string to use it.
  const std::string get_values_result = get_values_cb(keys).as<std::string>();
  const nlohmann::json get_values_json =
      nlohmann::json::parse(get_values_result, nullptr,
                            /*allow_exceptions=*/false,
                            /*ignore_comments=*/true);
  if (get_values_json.is_discarded() || !get_values_json.contains("kvPairs")) {
    return absl::InvalidArgumentError("No kvPairs returned");
  }

  emscripten::val kv_pairs = emscripten::val::object();
  for (auto& [k, v] : get_values_json["kvPairs"].items()) {
    if (v.contains("value")) {
      emscripten::val value = emscripten::val::object();
      value.set("value", emscripten::val(v["value"].get<std::string>()));
      kv_pairs.set(k, std::move(value));
    }
  }

  return kv_pairs;
}

emscripten::val getKeyGroupOutputs(const emscripten::val& get_values_cb,
                                   const emscripten::val& udf_arguments) {
  emscripten::val key_group_outputs = emscripten::val::array();
  // Convert a JS array to a std::vector so we can iterate through it.
  const std::vector<emscripten::val> key_groups =
      emscripten::vecFromJSArray<emscripten::val>(udf_arguments);
  for (auto key_group : key_groups) {
    emscripten::val key_group_output = emscripten::val::object();
    key_group_output.set("tags", key_group["tags"]);

    absl::StatusOr<emscripten::val> kv_pairs =
        getKvPairs(get_values_cb, key_group["data"]);
    if (kv_pairs.ok()) {
      key_group_output.set("keyValues", *kv_pairs);
      key_group_outputs.call<void>("push", key_group_output);
    }
  }
  return key_group_outputs;
}

}  // namespace

emscripten::val handleRequestCc(const emscripten::val& get_values_cb,
                                const emscripten::val& udf_arguments) {
  emscripten::val result = emscripten::val::object();
  result.set("keyGroupOutputs",
             getKeyGroupOutputs(get_values_cb, std::move(udf_arguments)));
  result.set("udfOutputApiVersion", emscripten::val(1));
  return result;
}

EMSCRIPTEN_BINDINGS(HandleRequestExample) {
  emscripten::function("handleRequestCc", &handleRequestCc);
}
