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

/**
 * Import the generated JS code based on public/udf/binary_get_values.proto
 */
goog.require('proto.kv_server.BinaryGetValuesResponse');

/**
 * @param {!Object} value
 * @param {!Object} keyGroupOutputs
 * @return {Object}
 * @suppress {reportUnknownTypes}
 */
function addValueToOutput(value, keyGroupOutputs) {
  let keyGroupOutput = value;
  keyGroupOutputs.push(keyGroupOutput);
  return null;
}

/**
 * @param {!Object} getValuesBinaryProto
 * @param {!Object} keyValuesOutput
 * @return {Object}
 * @suppress {reportUnknownTypes}
 */
function processGetValuesBinary(getValuesBinaryProto, keyValuesOutput) {
  var kvMap = getValuesBinaryProto.getKvPairsMap();
  const keys = kvMap.keys();
  for (const key of keys) {
    const val = kvMap.get(key);
    const hasData = val.hasData();
    if (hasData) {
      const valUint8Arr = val.getData_asU8();
      const valString = String.fromCharCode.apply(null, valUint8Arr);
      keyValuesOutput[key] = { value: valString };
    }
  }
  return keyValuesOutput;
}

/**
 * @param {!Array<!Array<string>>} lookupData
 * @return {Object}
 */
function callGetValuesBinary(lookupData) {
  let keyValuesOutput = {};
  for (const data of lookupData) {
    const getValuesUnparsed = getValuesBinary(data);
    const getValuesBinaryProto = proto.kv_server.BinaryGetValuesResponse.deserializeBinary(getValuesUnparsed);
    processGetValuesBinary(getValuesBinaryProto, keyValuesOutput);
  }
  keyValuesOutput['getValuesApi'] = 'getValuesBinary';
  return keyValuesOutput;
}

/**
 * @param {!Array<!Array<string>>} lookupData
 * @return {Object}
 */
function callGetValues(lookupData) {
  const keyValuesOutput = {};
  for (const data of lookupData) {
    const getValuesResult = JSON.parse(getValues(data));
    // getValuesResult returns "kvPairs" when successful and "code" on failure.
    // Ignore failures and only add successful getValuesResult lookups to
    // output.
    if (getValuesResult.hasOwnProperty('kvPairs')) {
      const kvPairs = getValuesResult['kvPairs'];
      for (const key in kvPairs) {
        if (kvPairs[key].hasOwnProperty('value')) {
          keyValuesOutput[key] = { value: kvPairs[key]['value'] };
        }
      }
    }
  }
  keyValuesOutput['getValuesApi'] = 'getValues';
  return keyValuesOutput;
}

/**
 * @param {!Array<string>} data
 * @param {number} batchSize
 * @return {!Array<!Array<string>>}
 */

function splitDataByBatchSize(data, batchSize) {
  const batches = [];
  for (let i = 0; i < data.length; i += batchSize) {
    batches.push(data.slice(i, i + batchSize));
  }
  return batches;
}

/**
 * @param {Object} requestMetadata
 * @param {!Array<Object>} udf_arguments
 * @return {Object}
 */
function handleGetValuesRequest(requestMetadata, udf_arguments) {
  let keyGroupOutputs = [];
  for (let argument of udf_arguments) {
    let keyGroupOutput = {};
    let data = argument.hasOwnProperty('tags') ? argument['data'] : argument;
    let lookupData = [data];
    if (requestMetadata.hasOwnProperty('lookup_batch_size')) {
      batchSize = parseInt(requestMetadata['lookup_batch_size'], 10);
      lookupData = splitDataByBatchSize(data, batchSize);
    }
    keyGroupOutput['keyValues'] = requestMetadataKeyIsTrue(requestMetadata, 'useGetValuesBinary')
      ? callGetValuesBinary(lookupData)
      : callGetValues(lookupData);
    keyGroupOutputs.push(keyGroupOutput);
  }
  return keyGroupOutputs;
}

function requestMetadataKeyIsTrue(requestMetadata, key) {
  return requestMetadata.hasOwnProperty(key) && requestMetadata[key] == true;
}

/**
 * Handles the runQuery flow:
 *   1. compute set union on arguments using `runQuery`
 *   2. getValues/getValuesBinary on first `lookup_n_keys_from_runquery` keys
 *   3. sort returned KVs by key length
 *   4. return top 5 KVs
 *
 * @param {!Object} requestMetadata
 * @param {!Array<Object>} udf_arguments
 * @returns {Object}
 */
function handleRunQueryFlow(requestMetadata, udf_arguments) {
  var result = {};
  result['udfOutputApiVersion'] = 1;
  if (!udf_arguments.length) {
    return result;
  }

  // Union all the sets in the udf_arguments
  const setKeys = udf_arguments[0].hasOwnProperty('data') ? udf_arguments[0]['data'] : udf_arguments[0];
  const keys = runQuery(setKeys.join('|'));
  const n = parseInt(requestMetadata['lookup_n_keys_from_runquery'] || '100', 10);

  const keyValuesOutput = handleGetValuesRequest(requestMetadata, [keys.slice(0, n)]);
  if (!keyValuesOutput.length || !keyValuesOutput[0].hasOwnProperty('keyValues')) {
    return result;
  }
  let top5keyValuesArray = Object.entries(keyValuesOutput[0]['keyValues'])
    .sort((a, b) => a[0].length - b[0].length)
    .slice(0, 5);
  result['keyGroupOutputs'] = Object.fromEntries(top5keyValuesArray);
  return result;
}

/**
 * Handles the getValues flow (without runQuery&sorting).
 *
 * @param {!Object} requestMetadata
 * @param {!Array<Object>} udf_arguments
 * @returns {Object}
 */
function handleGetValuesFlow(requestMetadata, udf_arguments) {
  const keyGroupOutputs = handleGetValuesRequest(requestMetadata, udf_arguments);
  var result = {};
  result['keyGroupOutputs'] = keyGroupOutputs;
  result['udfOutputApiVersion'] = 1;
  return result;
}

/**
 * Entry point for code snippet execution.
 *
 * This is a sample UDF js that calls different UDF APIs depending
 * on executionMetadata.requestMetadata fields.
 *
 * If `executionMetadata.requestMetadata.useGetValuesBinary` is set to true,
 * the UDF will use the `getValuesBinary` API to retrieve key-values.
 * Otherwise, the UDF will use the `getValues` API to retrieve key-values.
 *
 * If `executionMetadata.requestMetadata.runQuery` is set to true,
 * the UDF will do the following steps:
 *   1. Compute the set union of all elements in the first `udf_argument`
 *   2. Call `getValues`/`getValuesBinary` on the returned keys from step 1.
 *   3. Return the top 5 key values (sorted by key length) from step 2.
 * Otherwise, the UDF will just act as a pass-through UDF that calls
 * `getValues`/`getValuesBinary` on the `udf_arguments` and return the
 * retrieved key-value pairs.
 *
 * If `requestMetadata` has a `lookup_batch_size`, it will batch
 * getValues/getValuesBinary calls by the provided `lookup_batch_size`.
 * For example, if the input has 800 keys and `lookup_batch_size` is 300,
 * the UDF will make 3 getValues/getValuesBinary calls:
 *   2 call with 300 keys
 *   1 call with 200 keys
 *
 * The Closure Compiler will see the @export JSDoc annotation below and
 * generate the following synthetic code in the global scope:
 *
 *   goog.exportSymbol('HandleRequest', HandleRequest);
 *
 * That makes the minified version of this function still available in the
 * global namespace.
 *
 * You can also use @export for property names on classes, to prevent them
 * from being minified.
 *
 * @export
 * @param {Object} executionMetadata
 * @param {...!Object} udf_arguments
 * @return {Object}
 * @suppress {checkTypes}
 */
function HandleRequest(executionMetadata, ...udf_arguments) {
  const requestMetadata = executionMetadata.hasOwnProperty('requestMetadata')
    ? executionMetadata['requestMetadata']
    : {};

  if (requestMetadataKeyIsTrue(requestMetadata, 'runQuery')) {
    return handleRunQueryFlow(requestMetadata, udf_arguments);
  }
  return handleGetValuesFlow(requestMetadata, udf_arguments);
}
