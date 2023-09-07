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
#include "public/udf/binary_get_values.pb.h"
#include "tools/udf/inline_wasm/examples/get_values_binary_proto/stored_value.pb.h"

namespace {

// Assume that we have stored key-value objects in DELTA/SNAPSHOT files
// where the value is a serialized example::StoredValue proto
// (stored_value.proto).
// tools/udf/inline_wasm/examples/get_values_binary_proto/DELTA_1692813076489763
// contains values with serialized example::StoredValue proto
emscripten::val ProcessStoredValue(const kv_server::Value& value) {
  emscripten::val processed_value = emscripten::val::object();
  example::StoredValue my_proto;
  my_proto.ParseFromString(value.data());
  processed_value.set(
      "value", emscripten::val(std::move(*my_proto.mutable_my_string())));
  return processed_value;
}

// Calls getValuesBinary for a list of keys and processes the response.
absl::StatusOr<emscripten::val> getKvPairs(const emscripten::val& get_values_cb,
                                           const emscripten::val& keys) {
  // `get_values_cb` takes an emscripten::val of type array and returns
  // an emscripten::val of type Uint8Array that contains a serialized
  // BinaryGetValuesResponse. We can cast that directly to a string using
  // standard type conversions:
  // https://emscripten.org/docs/porting/connecting_cpp_and_javascript/embind.html#built-in-type-conversions
  std::string get_values_result = get_values_cb(keys).as<std::string>();

  emscripten::val kv_pairs = emscripten::val::object();
  kv_server::BinaryGetValuesResponse response;
  response.ParseFromArray(get_values_result.data(), get_values_result.size());
  for (auto& [k, v] : response.kv_pairs()) {
    kv_pairs.set(k, ProcessStoredValue(v));
  }
  return kv_pairs;
}

// I/O processing, similar to tools/udf/sample_udf/udf.js
emscripten::val getKeyGroupOutputs(const emscripten::val& get_values_cb,
                                   const emscripten::val& input) {
  emscripten::val key_group_outputs = emscripten::val::array();
  // Convert a JS array to a std::vector so we can iterate through it.
  const std::vector<emscripten::val> key_groups =
      emscripten::vecFromJSArray<emscripten::val>(input["keyGroups"]);
  for (auto key_group : key_groups) {
    emscripten::val key_group_output = emscripten::val::object();
    key_group_output.set("tags", key_group["tags"]);

    absl::StatusOr<emscripten::val> kv_pairs =
        getKvPairs(get_values_cb, key_group["keyList"]);
    if (kv_pairs.ok()) {
      key_group_output.set("keyValues", *kv_pairs);
      key_group_outputs.call<void>("push", key_group_output);
    }
  }
  return key_group_outputs;
}

}  // namespace

emscripten::val handleRequestCc(const emscripten::val& get_values_cb,
                                const emscripten::val& input) {
  emscripten::val result = emscripten::val::object();
  result.set("keyGroupOutputs",
             getKeyGroupOutputs(get_values_cb, std::move(input)));
  result.set("udfOutputApiVersion", emscripten::val(1));
  return result;
}

EMSCRIPTEN_BINDINGS(HandleRequestExample) {
  emscripten::function("handleRequestCc", &handleRequestCc);
}
