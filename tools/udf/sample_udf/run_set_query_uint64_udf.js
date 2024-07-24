/**
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

function HandleRequest(executionMetadata, ...input) {
  let keyGroupOutputs = [];
  for (const keyGroup of input) {
    let keyGroupOutput = {};
    if (!keyGroup.tags.includes('custom') || !keyGroup.tags.includes('queries')) {
      continue;
    }
    keyGroupOutput.tags = keyGroup.tags;
    if (!Array.isArray(keyGroup.data) || !keyGroup.data.length) {
      continue;
    }

    // Get the first key in the data.
    const runQueryArray = runSetQueryUInt64(keyGroup.data[0]);
    // runSetQueryInt returns an Uint8Array of 'uint64' ints and "code" on failure.
    // Ignore failures and only add successful runQuery results to output.
    if (runQueryArray instanceof Uint8Array) {
      const keyValuesOutput = {};
      const uint64Array = new BigUint64Array(bytes.buffer);
      const value = Array.from(uint64Array, (uint64) => uint64.toString());
      keyValuesOutput['result'] = { value: value };
      keyGroupOutput.keyValues = keyValuesOutput;
      keyGroupOutputs.push(keyGroupOutput);
    }
  }
  return { keyGroupOutputs, udfOutputApiVersion: 1 };
}
