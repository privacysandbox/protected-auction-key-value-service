# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@bazel_skylib//rules:run_binary.bzl", "run_binary")
load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")

def inline_wasm_udf_delta(
        name,
        wasm_binary,
        glue_js,
        custom_udf_js,
        custom_udf_js_handler = "HandleRequest",
        output_file_name = "DELTA_0000000000000005",
        udf_tool = "//tools/udf/udf_generator:udf_delta_file_generator",
        tags = ["manual"]):
    """Generate a JS + inline WASM UDF delta file and put it under dist/ directory

    Performs the following steps:
    1. Takes a wasm_binary and inlines it to JS.
    2. The inlined wasm + glue JS is prepended to the custom udf JS.
    3. The final JS file is used to generate a UDF delta file.

    Example usage:
        inline_wasm_udf_delta(
            name = "foo_delta",
            wasm_binary = "hello.wasm",
            glue_js = "hello.js",
            custom_udf_js = "my_udf.js",
            custom_udf_js_handler="HandleRequest",
            output_file_name="DELTA_0000000000000005"
        )

    Args:
        name: BUILD target name
        wasm_binary: WASM binary
        glue_js: Javascript glue code
        custom_udf_js: Custom UDF js to be included in the final JS
        custom_udf_js_handler: Handler name of custom UDF.
        output_file_name: Name of UDF delta file output.
            Recommended to follow DELTA file naming convention.
            Defaults to `DELTA_0000000000000005`
        udf_tool: build target for the udf_delta_file_generator.
            Defaults to `//tools/udf/udf_generator:udf_delta_file_generator`
        tags: tags to propagate to rules
    """

    getModule_js = """async function getModule(){
            var Module = {
            instantiateWasm: function (imports, successCallback) {
                var module = new WebAssembly.Module(wasm_array);
                var instance = new WebAssembly.Instance(module, imports);
                Module.testWasmInstantiationSucceeded = 1;
                successCallback(instance);
                return instance.exports;
            }
            };
            return await wasmModule(Module);
        }"""

    native.genrule(
        name = "{}_generated".format(name),
        srcs = [wasm_binary, glue_js, custom_udf_js],
        outs = ["{}_generated.js".format(name)],
        cmd_bash = """WASM_HEX=$$(
hexdump -v -e '1/1 "0x%02x,"' $(location {wasm_binary})
)
cat << EOF > $@
let wasm_array = new Uint8Array([$$WASM_HEX]);
$$(cat $(location {glue_js}))
{module_js}
$$(cat $(location {udf_js}))
EOF""".format(
            wasm_binary = wasm_binary,
            glue_js = glue_js,
            module_js = getModule_js,
            udf_js = custom_udf_js,
        ),
        visibility = ["//visibility:private"],
        tags = tags,
    )

    run_binary(
        name = "{}_udf_delta".format(name),
        srcs = [
            "{}_generated".format(name),
        ],
        outs = [
            output_file_name,
        ],
        args = [
            "--udf_file_path",
            "$(location {}_generated)".format(name),
            "--output_path",
            "$(location {})".format(output_file_name),
            "--udf_handler_name",
            custom_udf_js_handler,
        ],
        tool = udf_tool,
        visibility = ["//visibility:private"],
        tags = tags,
    )

    native.genrule(
        name = name,
        srcs = [
            "{}_udf_delta".format(name),
        ],
        outs = ["{}_copy_to_dist.bin".format(name)],
        cmd_bash = """cat << EOF > '$@'
mkdir -p dist/debian
cp $(location {}_udf_delta) dist
builders/tools/normalize-dist
EOF""".format(name),
        executable = True,
        local = True,
        message = "Copying {} dist directory".format(output_file_name),
        tags = tags,
    )

def cc_inline_wasm_udf_delta(
        name,
        cc_target,
        custom_udf_js,
        custom_udf_js_handler = "HandleRequest",
        output_file_name = "DELTA_0000000000000005",
        udf_tool = "//tools/udf/udf_generator:udf_delta_file_generator",
        tags = ["manual"]):
    """Generate a JS + inline WASM UDF delta file and put it under dist/ directory

    Performs the following steps:
    1. Takes a cc_target and uses emscripten to compile it to inline WASM + JS.
    2. The generated JS file is then prepended to the custom udf JS.
    3. The final JS file is used to generate a UDF delta file.

    Example usage:
        cc_inline_wasm_udf_delta(
            name = "foo_delta",
            cc_target = ":foo",
            custom_udf_js = "my_udf.js",
            custom_udf_js_handler="HandleRequest",
            output_file_name="DELTA_0000000000000005"
        )

    Args:
        name: BUILD target name
        cc_target: Name of the cc_target that will be compiled to WASM
        custom_udf_js: Custom UDF js to be included in the final JS
        custom_udf_js_handler: Handler name of custom UDF.
        output_file_name: Name of UDF delta file output.
            Recommended to follow DELTA file naming convention.
            Defaults to `DELTA_0000000000000005`
        udf_tool: build target for the udf_delta_file_generator.
            Defaults to `//tools/udf/udf_generator:udf_delta_file_generator`
        tags: tags to propagate to rules
    """

    # Generate WASM + JS using emscripten
    wasm_cc_binary(
        name = "{}_wasm_js_emscripten".format(name),
        cc_target = cc_target,
        outputs = [
            "{}_wasm_bin.wasm".format(name),
            "{}_glue.js".format(name),
        ],
        visibility = ["//visibility:private"],
        tags = tags,
    )

    inline_wasm_udf_delta(
        name = name,
        wasm_binary = ":{}_wasm_bin.wasm".format(name),
        glue_js = ":{}_glue.js".format(name),
        custom_udf_js = custom_udf_js,
        custom_udf_js_handler = custom_udf_js_handler,
        output_file_name = output_file_name,
        udf_tool = udf_tool,
        tags = tags,
    )
