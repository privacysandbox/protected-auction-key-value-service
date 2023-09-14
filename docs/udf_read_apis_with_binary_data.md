# UDF Datastore Read APIs with binary data

## Overview

The K/V server currently fully supports a
[datastore read API](https://github.com/privacysandbox/fledge-docs/blob/main/key_value_service_user_defined_functions.md#datastore-read-api)
that can be called from a UDF and returns a serialized JSON string with key-value pairs from the
datastore.

In addition to the existing `getValues` call, the server will add a `getValuesBinary` call that
returns a serialized [BinaryGetValuesResponse](/public/udf/binary_get_values.proto) protocol buffer
that contains key-value pairs with _binary_ values.

| Function name        | `getValues`     | `getValuesBinary`       |
| -------------------- | --------------- | ----------------------- |
| Input data type      | list of strings | list of strings         |
| Output data type     | serialized JSON | serizalized protobuf    |
| Parsed output format | JSON Object     | BinaryGetValuesResponse |

## Example snippets

### Example JS snippet calling `getValuesBinary`

The detailed example can be found in `//tools/udf/closure_js/examples/get_values_binary`.

Sample code snippet of how to parse the proto:

```js
goog.require("proto.kv_server.BinaryGetValuesResponse");


function myFunc(){
  ...
  var serializedGetValuesBinary = getValuesBinary(keyGroup["keyList"]);
  var getValuesBinaryProto = proto.kv_server.BinaryGetValuesResponse.deserializeBinary(serializedGetValuesBinary);
  ...
}
```

The example is compiled using [rules_closure](https://github.com/bazelbuild/rules_closure) and
contains a lot of closure-related annotations. You might want to address compilations warnings
rather than suppressing them for production code.

The closure compiler allows us to inline the protobuf library and create one output js file.

We first need to include the proto in the js library build target and create a `closure_js_binary`
target:

```bazel
closure_js_library(
    name = "example_js_lib",
    srcs = [
        "externs.js",  # Register APIs called from UDF as functions without definitions for closure.
        "udf.js",  # User-defined functions with handler/entrypoint
    ],
    convention = "NONE",
    deps = [
        "//public/udf:binary_get_values_js_proto",
    ],
)

closure_js_binary(
    name = "example_js",
    deps = [
        ":example_js_lib",
    ],
)

```

A few notes:

-   `externs.js` contains an empty function definition of `getValuesBinary` to register that
    function with the closure compilert
-   `udf.js` contains the actual custom code snippett

#### Closure js library to delta bazel macro

We also provide a macro for converting a `closure_js_library` target to a UDF delta file directly:

```bazel
closure_to_delta(
    name = "example_js_delta",
    closure_js_library_target = ":example_js_lib",
    output_file_name = "DELTA_0000000000000009",
)
```

Run the build target:

```shell
builders/tools/bazel-debian run //tools/udf/closure_js/examples/get_values_binary:example_js_delta
```

The delta file is in under `dist/`:

```shell
ls dist/DELTA_0000000000000009
```

### Example C++ (WASM w/ Emscripten) snippet calling `getValuesBinary`

The detailed example can be found in `//tools/udf/inline_wasm/examples/get_values_binary_proto`.

This explanation will only focus on the `getValuesBinary` call and how to invoke it from C++. For
how to generate a UDF js with inline WASM, see the guide on
[inline wasm UDFs](/docs/inline_wasm_udfs.md).

We first pass the `getValuesBinary` function from the JS wrapper to the C++ function.

JS:

```Javascript
  const result = module.handleRequestCc(getValuesBinary, input);
```

On the C++ side, accept the function as an input:

```C++
emscripten::val handleRequestCc(const emscripten::val& get_values_cb,
                                const emscripten::val& input) {
  ...
}
```

Use `get_values_cb` to call `getValuesBinary` and parse the string:

```C++
// `get_values_cb` takes an emscripten::val of type array and returns
// an emscripten::val of type Uint8Array that contains a serialized
// BinaryGetValuesResponse. We can cast that directly to a string using
// standard type conversions:
// https://emscripten.org/docs/porting/connecting_cpp_and_javascript/embind.html#built-in-type-conversions
std::string get_values_result = get_values_cb(keys).as<std::string>();

kv_server::BinaryGetValuesResponse response;
response.ParseFromArray(&get_values_result[0], get_values_result.size());
```
