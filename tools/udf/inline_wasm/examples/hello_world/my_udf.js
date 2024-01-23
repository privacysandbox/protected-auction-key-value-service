/**
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

function getKeyGroupOutputs(udf_arguments, module) {
  let keyGroupOutputs = [];
  for (const argument of udf_arguments) {
    let keyGroupOutput = {};
    keyGroupOutput.tags = argument.tags;
    let data = argument.hasOwnProperty('tags') ? argument.data : argument;
    const getValuesResult = JSON.parse(getValues(data));
    // getValuesResult returns "kvPairs" when successful and "code" on failure.
    // Ignore failures and only add successful getValuesResult lookups to output.
    if (getValuesResult.hasOwnProperty('kvPairs')) {
      const kvPairs = getValuesResult.kvPairs;
      const keyValuesOutput = {};
      for (const key in kvPairs) {
        if (kvPairs[key].hasOwnProperty('value')) {
          keyValuesOutput[key] = {
            value: kvPairs[key].value + module.HelloClass.SayHello('hi'),
          };
        }
      }
      keyGroupOutput.keyValues = keyValuesOutput;
      keyGroupOutputs.push(keyGroupOutput);
    }
  }
  return keyGroupOutputs;
}

async function HandleRequest(executionMetadata, ...udf_arguments) {
  logMessage('Handling request');
  const module = await getModule();
  logMessage('Done loading WASM Module');
  const keyGroupOutputs = getKeyGroupOutputs(udf_arguments, module);
  return { keyGroupOutputs, udfOutputApiVersion: 1 };
}
