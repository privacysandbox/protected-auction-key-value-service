# Compiling a WASM binary from C++

This example relies on [emsdk](https://github.com/emscripten-core/emsdk/tree/main/bazel).

To build a wasm binary from add.cc, simply build the wasm binary target:

```shell
bazel build //tools/wasm_example:add_wasm
```

This will output a list of wasm-related files. The `wasm` file is the binary.
