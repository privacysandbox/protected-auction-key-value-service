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

/**
 * Import the generated JS code based on public/udf/binary_get_values.proto
 */
goog.require("proto.kv_server.BinaryGetValuesResponse");

/**
 * @param {!Array} udf_arguments
 * @suppress {reportUnknownTypes}
 * @return {Object}
 */
function getKeyGroupOutputs(udf_arguments) {
    var keyGroupOutputs = [];
    for (const argument of udf_arguments) {
        var keyGroupOutput = {};
        keyGroupOutput["tags"] = argument["tags"];
        var data = argument.hasOwnProperty("tags") ? argument.data : argument ;

        // Get the bytes from the C++ side
        // The annotation below is meaningful since the compiler doesn't know the return type
        // of this function and it doesn't know the type of proto_as_bytes. So we tell it to
        // not worry about the return type and to treat proto_as_bytes as an Array
        /**
         * @type {!Array}
         * @suppress {reportUnknownTypes}
         */
        var serializedGetValuesBinary = /** @type {!Array} */ (getValuesBinary(data));
        var getValuesBinaryProto = proto.kv_server.BinaryGetValuesResponse.deserializeBinary(serializedGetValuesBinary);
        var kvMap = getValuesBinaryProto.getKvPairsMap();
        const keyValuesOutput = {};
        for (const key of kvMap.keys()) {
            if (kvMap.get(key).hasData()) {
                const valUint8Arr = kvMap.get(key).getData_asU8();
                const valString = String.fromCharCode.apply(null, valUint8Arr);
                keyValuesOutput[key] = { "value": valString };

            }
        }
        keyGroupOutput["keyValues"] = keyValuesOutput;
        keyGroupOutputs.push(keyGroupOutput);
    }
    return keyGroupOutputs;
}

/**
 * Entry point for code snippet execution.
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
 * @param {!Object} executionMetadata
 * @param {...?} udf_arguments
 * @return {Object}
 */
function HandleRequest(executionMetadata, ...udf_arguments) {
    const keyGroupOutputs = getKeyGroupOutputs(udf_arguments);
    return { "keyGroupOutputs": keyGroupOutputs, "udfOutputApiVersion": 1 };
}
