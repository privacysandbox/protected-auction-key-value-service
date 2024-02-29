// Copyright 2024 Google LLC
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

namespace {

// Calls getValues for a list of keys and processes the response.
absl::StatusOr<emscripten::val> getKvPairs(
    const emscripten::val& get_values_cb,
    const std::vector<emscripten::val>& lookup_data) {
  emscripten::val kv_pairs = emscripten::val::object();
  for (const auto& keys : lookup_data) {
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
    if (get_values_json.is_discarded() ||
        !get_values_json.contains("kvPairs")) {
      return absl::InvalidArgumentError("No kvPairs returned");
    }

    for (auto& [k, v] : get_values_json["kvPairs"].items()) {
      if (v.contains("value")) {
        emscripten::val value = emscripten::val::object();
        value.set("value", emscripten::val(v["value"].get<std::string>()));
        kv_pairs.set(k, value);
      }
    }
  }

  kv_pairs.set("udfApi", "getValues");
  return kv_pairs;
}

// Calls getValuesBinary for a list of keys and processes the response.
absl::StatusOr<emscripten::val> getKvPairsBinary(
    const emscripten::val& get_values_binary_cb,
    const std::vector<emscripten::val>& lookup_data) {
  emscripten::val kv_pairs = emscripten::val::object();
  for (const auto& keys : lookup_data) {
    // `get_values_cb` takes an emscripten::val of type array and returns
    // an emscripten::val of type Uint8Array that contains a serialized
    // BinaryGetValuesResponse.
    std::vector<uint8_t> get_values_binary_result_array =
        emscripten::convertJSArrayToNumberVector<uint8_t>(
            get_values_binary_cb(keys));
    kv_server::BinaryGetValuesResponse response;
    response.ParseFromArray(get_values_binary_result_array.data(),
                            get_values_binary_result_array.size());
    for (auto& [k, v] : response.kv_pairs()) {
      kv_pairs.set(k, v.data());
    }
  }
  kv_pairs.set("udfApi", "getValuesBinary");
  return kv_pairs;
}

std::vector<emscripten::val> maybeSplitDataByBatchSize(
    const emscripten::val& execution_metadata, const emscripten::val& data) {
  if (!execution_metadata.hasOwnProperty("lookup_batch_size")) {
    return std::vector<emscripten::val>({data});
  }
  const int batch_size = execution_metadata["lookup_batch_size"].as<int>();
  const int data_length = data["length"].as<int>();
  if (batch_size >= data_length) {
    return std::vector<emscripten::val>({data});
  }
  std::vector<emscripten::val> batches;
  for (int i = 0; i < data_length; i += batch_size) {
    batches.emplace_back(
        data.call<emscripten::val>("slice", i, i + batch_size));
  }
  return batches;
}

// I/O processing, similar to
// tools/latency_benchmarking/example/udf_code/benchmark_udf.js
emscripten::val getKeyGroupOutputs(const emscripten::val& get_values_cb,
                                   const emscripten::val& get_values_binary_cb,
                                   const emscripten::val& execution_metadata,
                                   const emscripten::val& udf_arguments,
                                   bool use_binary) {
  emscripten::val key_group_outputs = emscripten::val::array();
  // Convert a JS array to a std::vector so we can iterate through it.
  const std::vector<emscripten::val> key_groups =
      emscripten::vecFromJSArray<emscripten::val>(udf_arguments);
  for (const auto& key_group : key_groups) {
    emscripten::val key_group_output = emscripten::val::object();
    key_group_output.set("tags", key_group["tags"]);
    const emscripten::val data =
        key_group.hasOwnProperty("tags") ? key_group["data"] : key_group;

    std::vector<emscripten::val> lookup_data =
        maybeSplitDataByBatchSize(execution_metadata, data);
    absl::StatusOr<emscripten::val> kv_pairs;
    kv_pairs = use_binary ? getKvPairsBinary(get_values_binary_cb, lookup_data)
                          : getKvPairs(get_values_cb, lookup_data);
    if (kv_pairs.ok()) {
      key_group_output.set("keyValues", *kv_pairs);
      key_group_outputs.call<void>("push", key_group_output);
    }
  }
  return key_group_outputs;
}

absl::StatusOr<bool> useBinary(const emscripten::val& execution_metadata) {
  if (!execution_metadata.hasOwnProperty("udfApi")) {
    return false;
  }
  const std::string udf_api = execution_metadata["udfApi"].as<std::string>();
  if (udf_api == "getValues") {
    return false;
  }
  if (udf_api == "getValuesBinary") {
    return true;
  }
  return absl::InvalidArgumentError("unsupported udfApi");
}

}  // namespace

emscripten::val handleRequestCc(const emscripten::val& get_values_cb,
                                const emscripten::val& get_values_binary_cb,
                                const emscripten::val& execution_metadata,
                                const emscripten::val& udf_arguments) {
  emscripten::val result = emscripten::val::object();
  emscripten::val key_group_outputs;
  const auto use_binary_or_status = useBinary(execution_metadata);
  if (use_binary_or_status.ok()) {
    key_group_outputs = getKeyGroupOutputs(get_values_cb, get_values_binary_cb,
                                           execution_metadata, udf_arguments,
                                           *use_binary_or_status);
  } else {
    key_group_outputs = emscripten::val::object();
    key_group_outputs.set("error", "unsupported udfApi");
  }
  result.set("keyGroupOutputs", key_group_outputs);
  result.set("udfOutputApiVersion", emscripten::val(1));
  return result;
}

EMSCRIPTEN_BINDINGS(HandleRequestExample) {
  emscripten::function("handleRequestCc", &handleRequestCc);
}
