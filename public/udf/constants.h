// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

namespace kv_server {

constexpr char kUdfCodeSnippetKey[] = "udf_code_snippet";
constexpr char kUdfHandlerNameKey[] = "udf_handler_name";

constexpr char kDefaultUdfCodeSnippet[] = R"(
    function HandleRequest(input) {
  const keyGroupOutputs = [];
  for (const keyGroup of input.keyGroups) {
    const keyGroupOutput = {};
    keyGroupOutput.tags = keyGroup.tags;

    const kvPairs = JSON.parse(getValues(keyGroup.keyList)).kvPairs;
    const keyValuesOutput = {};
    for (const key in kvPairs) {
      if (kvPairs[key].hasOwnProperty("value")) {
        keyValuesOutput[key] = { "value": kvPairs[key].value };
      }
    }
    keyGroupOutput.keyValues = keyValuesOutput;
    keyGroupOutputs.push(keyGroupOutput);
  }
  return {keyGroupOutputs, udfOutputApiVersion: 1};
}
)";
constexpr char kDefaultUdfHandlerName[] = "HandleRequest";

}  // namespace kv_server
