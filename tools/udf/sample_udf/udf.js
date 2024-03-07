/**
 * Copyright 2022 Google LLC
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

function getKeyGroupOutputs(udf_arguments) {
  let keyGroupOutputs = [];
  for (let argument of udf_arguments) {
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
          keyValuesOutput[key] = { value: kvPairs[key].value };
        }
      }
      keyGroupOutput.keyValues = keyValuesOutput;
      keyGroupOutputs.push(keyGroupOutput);
    }
  }
  return keyGroupOutputs;
}

function handlePas(udf_arguments) {
  if (udf_arguments.length != 1) {
    let error_message =
      'For PAS default UDF exactly one argument should be provided, but was provided ' + udf_arguments.length;
    console.error(error_message);
    return error_message;
  }
  const kv_result = JSON.parse(getValues(udf_arguments[0]));
  if (kv_result.hasOwnProperty('kvPairs')) {
    return kv_result.kvPairs;
  }
  let error_message_lookup = 'Failed looking up values';
  console.error(error_message_lookup);
  return error_message_lookup;
}

function handlePA(udf_arguments) {
  const keyGroupOutputs = getKeyGroupOutputs(udf_arguments);
  return { keyGroupOutputs, udfOutputApiVersion: 1 };
}

function HandleRequest(executionMetadata, ...udf_arguments) {
  if (executionMetadata.requestMetadata && executionMetadata.requestMetadata.is_pas) {
    console.log('Executing PAS branch');
    return handlePas(udf_arguments);
  }
  return handlePA(udf_arguments);
}
