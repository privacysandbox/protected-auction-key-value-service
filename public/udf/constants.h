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

constexpr int64_t kDefaultLogicalCommitTime = 0;
// Roma only allows version_num > 0;
constexpr int64_t kDefaultVersion = 1;

constexpr char kDefaultUdfCodeSnippet[] = R"(
function getKeyGroupOutputs(udf_arguments) {
  let keyGroupOutputs = [];
  for (let argument of udf_arguments) {
    let keyGroupOutput = {};
    let data = argument;
    if (argument.hasOwnProperty("tags")) {
      keyGroupOutput.tags = argument.tags;
      if (argument.hasOwnProperty("data")) {
        data = argument.data;
      } else {
        continue;
      }
    }
    const getValuesResult = JSON.parse(getValues(data));
    // getValuesResult returns "kvPairs" when successful and "code" on failure.
    // Ignore failures and only add successful getValuesResult lookups to output.
    if (getValuesResult.hasOwnProperty("kvPairs")) {
      const kvPairs = getValuesResult.kvPairs;
      const keyValuesOutput = {};
      for (const key in kvPairs) {
        if (kvPairs[key].hasOwnProperty("value")) {
          keyValuesOutput[key] = { "value": kvPairs[key].value };
        }
      }
      keyGroupOutput.keyValues = keyValuesOutput;
      keyGroupOutputs.push(keyGroupOutput);
    }
  }
  return keyGroupOutputs;
}


function HandleRequest(executionMetadata, ...udf_arguments) {
  const keyGroupOutputs = getKeyGroupOutputs(udf_arguments);
  return {keyGroupOutputs, udfOutputApiVersion: 1};
}
)";

constexpr char kDefaultUdfHandlerName[] = "HandleRequest";

}  // namespace kv_server
