/**
 * Copyright 2024 Google LLC
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

function getKeyGroupOutputs(udf_arguments, batchLogMetrics) {
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
          ++batchLogMetrics.udf_metric[2].value;
        }
      }
      keyGroupOutput.keyValues = keyValuesOutput;
      keyGroupOutputs.push(keyGroupOutput);
    }
  }
  return keyGroupOutputs;
}

function handlePas(udf_arguments, batchLogMetrics) {
  if (udf_arguments.length != 1) {
    let error_message =
      'For PAS default UDF exactly one argument should be provided, but was provided ' + udf_arguments.length;
    console.error(error_message);
    ++batchLogMetrics.udf_metric[0].value;
    return error_message;
  }
  const kv_result = JSON.parse(getValues(udf_arguments[0]));
  if (kv_result.hasOwnProperty('kvPairs')) {
    return kv_result.kvPairs;
  }
  let error_message_lookup = 'Failed looking up values';
  console.error(error_message_lookup);
  ++batchLogMetrics.udf_metric[1].value;
  return error_message_lookup;
}

function handlePA(udf_arguments, batchLogMetrics) {
  const keyGroupOutputs = getKeyGroupOutputs(udf_arguments, batchLogMetrics);
  return { keyGroupOutputs, udfOutputApiVersion: 1 };
}

// 1. The telemetry_config that defines the custom metrics instrumented in the UDF need to be passed to the server parameter.
// Example telemetry_config to log custom metrics instrumented in this UDF example:
// mode: PROD custom_udf_metric { name: \"udf_argument_error_count\" description: \"udf argument error count\" lower_bound: 0 upper_bound: 2 } custom_udf_metric { name: \"lookup_failure_count\" description: \"lookup failure count\" lower_bound: 0 upper_bound: 10 } custom_udf_metric { name: \"key_with_value_count\" description: \"key with value count\" lower_bound: 0 upper_bound: 100 }
// 2. Currently we only support logging UDF custom metrics up to 3 counter metrics
// 3. If a metric is loggged multiple times for a given request, the value will be accumulated and the sum will be published as final value.
function HandleRequest(executionMetadata, ...udf_arguments) {
  let udfArgumentsErrorCount = {
    name: 'udf_argument_error_count',
    value: 0,
  };
  let lookupFailureCount = {
    name: 'lookup_failure_count',
    value: 0,
  };
  let keyWithValueCount = {
    name: 'key_with_value_count',
    value: 0,
  };
  let batchLogMetrics = {
    udf_metric: [udfArgumentsErrorCount, lookupFailureCount, keyWithValueCount],
  };
  if (executionMetadata.requestMetadata && executionMetadata.requestMetadata.is_pas) {
    logMessage('Executing PAS branch');
    const result = handlePas(udf_arguments, batchLogMetrics);
    logCustomMetric(JSON.stringify(batchLogMetrics));
    return result;
  }
  const result = handlePA(udf_arguments, batchLogMetrics);
  logCustomMetric(JSON.stringify(batchLogMetrics));
  return result;
}
