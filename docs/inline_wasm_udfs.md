# Getting Started with inline WASM (C++) UDFs

This is a sample guide for how to generate a UDF JS with inline WASM from C++.

## Overview

We will be using a provided bazel macro under
[`//tools/udf/inline_wasm/wasm.bzl`](/tools/udf/inline_wasm/wasm.bzl) to generate a UDF delta file.

The macro uses
[Embind](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/embind.html) to generate
a WASM binary and its accompanying JavaScript.

Using Embind requires including C++ code that exposes C++ to JavaScript. Similarly, the JavaScript
needs to instantiate a WASM module and access it through a variable.

This guide will not cover usage of Emscripten/Embind beyond what is needed for the examples under
`//tools/udf/inline_wasm/examples`. Please browse through the
[Emscripten documentation](https://emscripten.org/docs/introducing_emscripten/index.html) for
detailed usage.

## Memory limits

By default, Emscripten sets the default WASM memory limit to 16MB.

This can be adjusted by compiling the WASM with certain linkopts such as
`s INITIAL_MEMORY=<initial memory size>`, `-s ALLOW_MEMORY_GROWTH=1`,
`-s MAXIMUM_MEMORY=<maximum memory size>`

The maximum memory that the K/V server's JS engine will allow for WASM is 4GB.

See
[Emscripten documentation](https://emscripten.org/docs/api_reference/module.html?highlight=initial_memory#Module.wasmMemory)
for more info on WASM memory.

## Example

Sample code is at `//tools/udf/inline_wasm/examples/hello_world`.

### Write C++ code

For our example, we have a simple `HelloClass` with a static `SayHello` function.

```C++
#include "emscripten/bind.h"

class HelloClass {
 public:
  static std::string SayHello(const std::string& name) { return "Yo! " + name; }
};

EMSCRIPTEN_BINDINGS(Hello) {
  emscripten::class_<HelloClass>("HelloClass")
      .constructor<>()
      .class_function("SayHello", &HelloClass::SayHello);
}
```

### Write JS code

The provided bazel macro will include the following function in the final JavaScript output.

```javascript
async function getModule() {
    var Module = {
        instantiateWasm: function (imports, successCallback) {
            var module = new WebAssembly.Module(wasm_array);
            var instance = new WebAssembly.Instance(module, imports);
            Module.testWasmInstantiationSucceeded = 1;
            successCallback(instance);
            return instance.exports;
        },
    };
    return await wasmModule(Module);
}
```

You will need to call this function from your custom UDF to access your C++ bindings.

Example:

```javascript
const module = await getModule();
module.HelloClass.SayHello('hi');
```

### Generate a UDF delta file

#### C++

We have a bazel macro for generating a UDF file given a C++ source file and a custom UDF JavaScript.
The output will be under the `dist/` directory.

The macro will

1. Compile an inline WASM binary with JS glue
2. Appends the custom JS
3. Generates a UDF delta file

-   Define your BUILD target:

```bazel
load("//tools/udf/inline_wasm:wasm.bzl", "cc_inline_wasm_udf_delta")

cc_inline_wasm_udf_delta (
    name = "hello_delta",
    srcs = ["hello.cc"],
    deps = [
      ":hello_deps",
    ],
    custom_udf_js = "my_udf.js",
    custom_udf_js_handler = "HandleRequest",
    output_file_name = "DELTA_0000000000000001",
    linkopts = ["-s ALLOW_MEMORY_GROWTH=1", "-s MAXIMUM_MEMORY=1000MB"]
)
```

The macro compiles the WASM binary with a set of
[default linkopts](https://github.com/privacysandbox/data-plane-shared-libraries/blob/dad1d78eaffc0e74eb70090cb3a5560166d5f4c6/build_defs/cc/wasm.bzl#L18).
Additional linkopts can be passed in through the `linkopts` parameter.

-   Generate the DELTA file:

```shell
builders/tools/bazel-debian run --incompatible_enable_cc_toolchain_resolution path/to/udf_delta_target:hello_delta
```

-   Get the DELTA file from `dist/`.

#### Non-C++

We have a bazel macro for generating a UDF file given a wasm binary and glue JS generated by
emscripten. The output will be under the `dist/` directory.

-   Define your BUILD target:

```bazel
load("//tools/udf/inline_wasm:wasm.bzl", "inline_wasm_udf_delta")

inline_wasm_udf_delta(
    name = "hello_delta",
    wasm_binary = "hello.wasm",
    glue_js = "hello.js",
    custom_udf_js = "my_udf.js",
    custom_udf_js_handler="HandleRequest",
    output_file_name="DELTA_0000000000000005"
)
```

-   Generate the DELTA file:

```shell
builders/tools/bazel-debian run path/to/udf_delta_target:hello_delta
```

-   The DELTA file should now be under `dist/`.

```shell
ls dist/DELTA_*
```

### Test the UDF delta file

To test the UDF delta file, use the provided UDF tools.

1. Build the UDF testing tool executables:

    ```sh
    -$ ./builders/tools/bazel-debian build --config nonprod_mode //tools/udf/udf_tester:udf_delta_file_tester
    ```

1. Have a delta/snapshot file with key-value pairs ready. If you don't have one, you can use
   `./tools/serving_data_generator/generate_test_riegeli_data` to generate one.

    ```sh
    KV_DELTA=path/to/kv/delta/file
    ```

1. Define a test key. This should be a key you want to include as an input to the UDF:

    ```sh
    TEST_KEY=foo1
    ```

1. Run the `udf_delta_file_tester` which run the UDF provided under with the given argument in the
   input.

    ```sh
    UDF_DELTA=path/to/udf/delta
    bazel-bin/tools/udf/udf_tester/udf_delta_file_tester  --input_arguments="$TEST_KEY" --kv_delta_file_path="$KV_DELTA" --udf_delta_file_path="$UDF_DELTA" --v=10 --stderrthreshold=0
    ```

    See the [generating UDF files doc](./generating_udf_files.md#3-test-the-udf-delta-file) for more
    options.

Repeat the last step whenever you change your inline WASM and want to test it.

## Calling UDF APIs from C++ WASM

To call APIs available to the UDF from C++, the custom JavaScript code needs to pass the function as
an input to the C++ WASM.

For example, to pass the `getValues` function to C++,

```javascript
// Pass in the getValues function for the C++ code to call.
const result = module.handleRequestCc(getValues, input);
```

On the C++ side, `getValues` is an `emscripten::val`:

```C++
emscripten::val handleRequestCc(const emscripten::val& get_values_cb,
                                const emscripten::val& udf_arguments) {
    ...
}
```

`get_values_cb` can be called with an `emscripten::val` of type `array`. The result will be a
serialized JSON.

```C++

emscripten::val keys = emscripten::val::array();
...

const std::string get_values_result = get_values_cb(keys).as<std::string>();

```

For a full example see `//tools/udf/inline_wasm/examples/js_call`.
