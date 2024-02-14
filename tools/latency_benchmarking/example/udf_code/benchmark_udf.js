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
  keyValuesOutput['udfApi'] = 'getValuesBinary';
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
  keyValuesOutput['udfApi'] = 'getValues';
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
 * @param {boolean} useBinary
 * @param {Object} requestMetadata
 * @param {!Array<Object>} udf_arguments
 * @return {Object}
 */
function handleGetValuesRequest(useBinary, requestMetadata, udf_arguments) {
  let keyGroupOutputs = [];
  for (let argument of udf_arguments) {
    let keyGroupOutput = {};
    let data = argument.hasOwnProperty('tags') ? argument['data'] : argument;
    let lookupData = [data];
    if (requestMetadata.hasOwnProperty('lookup_batch_size')) {
      batchSize = parseInt(requestMetadata['lookup_batch_size'], 10);
      lookupData = splitDataByBatchSize(data, batchSize);
    }
    keyGroupOutput['keyValues'] = useBinary ? callGetValuesBinary(lookupData) : callGetValues(lookupData);
    keyGroupOutputs.push(keyGroupOutput);
  }
  return keyGroupOutputs;
}

function getKeyGroupOutputs(executionMetadata, udf_arguments) {
  if (!executionMetadata.hasOwnProperty('requestMetadata')) {
    return handleGetValuesRequest(false, {}, udf_arguments);
  }

  const requestMetadata = executionMetadata['requestMetadata'];
  if (!requestMetadata.hasOwnProperty('udfApi')) {
    return handleGetValuesRequest(false, requestMetadata, udf_arguments);
  }

  if (requestMetadata['udfApi'] == 'getValues') {
    return handleGetValuesRequest(false, requestMetadata, udf_arguments);
  }
  if (requestMetadata['udfApi'] == 'getValuesBinary') {
    return handleGetValuesRequest(true, requestMetadata, udf_arguments);
  }
  return { error: 'unsupported udfApi ' + requestMetadata['udfApi'] };
}

/**
 * Entry point for code snippet execution.
 *
 * This is a pass-through UDF js that calls either
 * getValues or getValuesBinary, depending on
 * `executionMetadata.requestMetadata.udfApi`.
 *
 * If `requestMetadata` has a `lookup_batch_size`, it will batch
 * calls by the provided `lookup_batch_size`.
 * For example, if the UDF input has 800 keys and `lookup_batch_size` is 300,
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
  const keyGroupOutputs = getKeyGroupOutputs(executionMetadata, udf_arguments);
  var result = {};
  result['keyGroupOutputs'] = keyGroupOutputs;
  result['udfOutputApiVersion'] = 1;
  return result;
}
