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

constexpr int kTopK = 4;
constexpr int kLookupNKeysFromRunQuery = 100;

// Calls getValues for a list of keys and processes the response.
absl::StatusOr<std::vector<nlohmann::json>> GetKvPairs(
    const emscripten::val& get_values_cb,
    const std::vector<emscripten::val>& lookup_data) {
  std::vector<nlohmann::json> kv_pairs_json;
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
    kv_pairs_json.push_back(get_values_json);
  }
  return kv_pairs_json;
}

absl::StatusOr<emscripten::val> GetKvPairsAsEmVal(
    const emscripten::val& get_values_cb,
    const std::vector<emscripten::val>& lookup_data) {
  emscripten::val kv_pairs = emscripten::val::object();
  kv_pairs.set("udfApi", "getValues");

  const auto kv_pairs_json_or_status = GetKvPairs(get_values_cb, lookup_data);
  if (!kv_pairs_json_or_status.ok()) {
    return kv_pairs_json_or_status.status();
  }
  for (const auto& kv_pair_json : *kv_pairs_json_or_status) {
    for (auto& [k, v] : kv_pair_json["kvPairs"].items()) {
      if (v.contains("value")) {
        emscripten::val value = emscripten::val::object();
        value.set("value", emscripten::val(v["value"].get<std::string>()));
        kv_pairs.set(k, value);
      }
    }
  }
  return kv_pairs;
}

absl::StatusOr<std::map<std::string, std::string>> GetKvPairsAsMap(
    const emscripten::val& get_values_cb,
    const std::vector<emscripten::val>& lookup_data) {
  std::map<std::string, std::string> kv_map;
  kv_map["udfApi"] = "getValues";

  const auto kv_pairs_json_or_status = GetKvPairs(get_values_cb, lookup_data);
  if (!kv_pairs_json_or_status.ok()) {
    return kv_pairs_json_or_status.status();
  }
  for (const auto& kv_pair_json : *kv_pairs_json_or_status) {
    for (auto& [k, v] : kv_pair_json["kvPairs"].items()) {
      if (v.contains("value")) {
        kv_map[k] = v["value"];
      }
    }
  }
  return kv_map;
}

// Calls getValuesBinary for a list of keys and processes the response.
absl::StatusOr<std::vector<kv_server::BinaryGetValuesResponse>>
GetKvPairsBinary(const emscripten::val& get_values_binary_cb,
                 const std::vector<emscripten::val>& lookup_data) {
  std::vector<kv_server::BinaryGetValuesResponse> responses;
  for (const auto& keys : lookup_data) {
    // `get_values_cb` takes an emscripten::val of type array and returns
    // an emscripten::val of type Uint8Array that contains a serialized
    // BinaryGetValuesResponse.
    std::vector<uint8_t> get_values_binary_result_array =
        emscripten::convertJSArrayToNumberVector<uint8_t>(
            get_values_binary_cb(keys));
    kv_server::BinaryGetValuesResponse response;
    if (!response.ParseFromArray(get_values_binary_result_array.data(),
                                 get_values_binary_result_array.size())) {
      return absl::InternalError(
          absl::StrCat("Could not parse binary response to proto"));
    }
    responses.push_back(response);
  }
  return responses;
}

absl::StatusOr<emscripten::val> GetKvPairsBinaryAsEmVal(
    const emscripten::val& get_values_binary_cb,
    const std::vector<emscripten::val>& lookup_data) {
  emscripten::val kv_pairs = emscripten::val::object();
  kv_pairs.set("udfApi", "getValuesBinary");
  const auto responses_or_status =
      GetKvPairsBinary(get_values_binary_cb, lookup_data);
  if (!responses_or_status.ok()) {
    return responses_or_status.status();
  }
  for (const auto& response : *responses_or_status) {
    for (auto& [k, v] : response.kv_pairs()) {
      kv_pairs.set(k, v.data());
    }
  }
  return kv_pairs;
}

absl::StatusOr<std::map<std::string, std::string>> GetKvPairsBinaryAsMap(
    const emscripten::val& get_values_binary_cb,
    const std::vector<emscripten::val>& lookup_data) {
  std::map<std::string, std::string> kv_map;
  kv_map["udfApi"] = "getValuesBinary";
  const auto responses_or_status =
      GetKvPairsBinary(get_values_binary_cb, lookup_data);
  if (!responses_or_status.ok()) {
    return responses_or_status.status();
  }
  for (const auto& response : *responses_or_status) {
    for (auto& [k, v] : response.kv_pairs()) {
      kv_map[k] = v.data();
    }
  }
  return kv_map;
}

std::vector<emscripten::val> MaybeSplitDataByBatchSize(
    const emscripten::val& request_metadata, const emscripten::val& data) {
  if (!request_metadata.hasOwnProperty("lookup_batch_size")) {
    return std::vector<emscripten::val>({data});
  }
  const int batch_size = request_metadata["lookup_batch_size"].as<int>();
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
// tools/benchmarking/example/udf_code/benchmark_udf.js
emscripten::val GetKeyGroupOutputs(const emscripten::val& get_values_cb,
                                   const emscripten::val& get_values_binary_cb,
                                   const emscripten::val& request_metadata,
                                   const emscripten::val& udf_arguments) {
  emscripten::val key_group_outputs = emscripten::val::array();
  // Convert a JS array to a std::vector so we can iterate through it.
  const std::vector<emscripten::val> key_groups =
      emscripten::vecFromJSArray<emscripten::val>(udf_arguments);
  for (const auto& key_group : key_groups) {
    emscripten::val key_group_output = emscripten::val::object();
    const emscripten::val data =
        key_group.hasOwnProperty("tags") ? key_group["data"] : key_group;

    std::vector<emscripten::val> lookup_data =
        MaybeSplitDataByBatchSize(request_metadata, data);
    absl::StatusOr<emscripten::val> kv_pairs;
    kv_pairs = request_metadata.hasOwnProperty("useGetValuesBinary") &&
                       request_metadata["useGetValuesBinary"].as<bool>()
                   ? GetKvPairsBinaryAsEmVal(get_values_binary_cb, lookup_data)
                   : GetKvPairsAsEmVal(get_values_cb, lookup_data);
    if (kv_pairs.ok()) {
      key_group_output.set("keyValues", *kv_pairs);
      key_group_outputs.call<void>("push", key_group_output);
    }
  }
  return key_group_outputs;
}

emscripten::val HandleGetValuesFlow(const emscripten::val& get_values_cb,
                                    const emscripten::val& get_values_binary_cb,
                                    const emscripten::val& request_metadata,
                                    const emscripten::val& udf_arguments) {
  emscripten::val result = emscripten::val::object();
  emscripten::val key_group_outputs = GetKeyGroupOutputs(
      get_values_cb, get_values_binary_cb, request_metadata, udf_arguments);
  result.set("keyGroupOutputs", key_group_outputs);
  result.set("udfOutputApiVersion", emscripten::val(1));
  return result;
}

// The run query flow performs the following steps:
// 1. Compute the set union of given arguments using `runQuery` API
// 2. Call `getValues`/`getValuesBinary` with first
// `lookup_n_keys_from_runquery` keys
// 3. Sort returned KVs
// 4. Return top 5 KV pairs
emscripten::val HandleRunQueryFlow(const emscripten::val& get_values_cb,
                                   const emscripten::val& get_values_binary_cb,
                                   const emscripten::val& run_query_cb,
                                   const emscripten::val& request_metadata,
                                   const emscripten::val& udf_arguments) {
  emscripten::val result = emscripten::val::object();
  result.set("udfOutputApiVersion", emscripten::val(1));
  if (udf_arguments["length"].as<int>() <= 0) {
    return result;
  }
  // Union of all sets in udf_arguments
  emscripten::val set_keys = udf_arguments[0].hasOwnProperty("data")
                                 ? udf_arguments[0]["data"]
                                 : udf_arguments[0];
  emscripten::val query =
      set_keys.call<emscripten::val>("join", emscripten::val("|"));
  emscripten::val keys = run_query_cb(query);
  int n = kLookupNKeysFromRunQuery;
  if (request_metadata.hasOwnProperty("lookup_n_keys_from_runquery")) {
    n = request_metadata["lookup_n_keys_from_runquery"].as<int>();
  }
  emscripten::val lookup_keys = keys.call<emscripten::val>("slice", 0, n);
  std::vector<emscripten::val> lookup_data =
      MaybeSplitDataByBatchSize(request_metadata, lookup_keys);
  absl::StatusOr<std::map<std::string, std::string>> kv_map =
      request_metadata.hasOwnProperty("useGetValuesBinary") &&
              request_metadata["useGetValuesBinary"].as<bool>()
          ? GetKvPairsBinaryAsMap(get_values_binary_cb, lookup_data)
          : GetKvPairsAsMap(get_values_cb, lookup_data);
  if (!kv_map.ok()) {
    return result;
  }
  int i = 0;
  emscripten::val key_group_outputs = emscripten::val::array();
  emscripten::val key_group_output = emscripten::val::object();
  emscripten::val kv_pairs = emscripten::val::object();
  // select only kTopK
  for (const auto& [k, v] : *kv_map) {
    emscripten::val value = emscripten::val::object();
    value.set("value", v);
    kv_pairs.set(k, value);
    if (++i > kTopK) {
      break;
    }
  }
  key_group_output.set("keyValues", kv_pairs);
  key_group_outputs.call<void>("push", key_group_output);
  result.set("keyGroupOutputs", key_group_outputs);
  return result;
}

}  // namespace

emscripten::val HandleRequestCc(const emscripten::val& get_values_cb,
                                const emscripten::val& get_values_binary_cb,
                                const emscripten::val& run_query_cb,
                                const emscripten::val& request_metadata,
                                const emscripten::val& udf_arguments) {
  if (request_metadata.hasOwnProperty("runQuery") &&
      request_metadata["runQuery"].as<bool>()) {
    return HandleRunQueryFlow(get_values_cb, get_values_binary_cb, run_query_cb,
                              request_metadata, udf_arguments);
  }
  return HandleGetValuesFlow(get_values_cb, get_values_binary_cb,
                             request_metadata, udf_arguments);
}

EMSCRIPTEN_BINDINGS(HandleRequestExample) {
  emscripten::function("handleRequestCc", &HandleRequestCc);
}
