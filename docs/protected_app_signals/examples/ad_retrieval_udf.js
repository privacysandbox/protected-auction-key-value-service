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

function HandleRequest(requestMetadata, protectedSignals, deviceMetadata, contextualSignals, contextualAdIds) {
  let protectedSignalsKeys = [];
  const parsedProtectedSignals = JSON.parse(protectedSignals);
  for (const [key, value] of Object.entries(parsedProtectedSignals)) {
    protectedSignalsKeys.push(key);
  }
  const kv_result = JSON.parse(getValues(protectedSignalsKeys));
  if (kv_result.hasOwnProperty('kvPairs')) {
    return kv_result.kvPairs;
  }
  const error_message = 'Error executing handle PAS:' + JSON.stringify(kv_result);
  console.error(error_message);
  throw new Error(error_message);
}
