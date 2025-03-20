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
load("@google_privacysandbox_servers_common//build_defs/cc:wasm.bzl", "inline_wasm_cc_binary", "inline_wasm_udf_js")

def inline_wasm_module_udf_js(
        name,
        glue_js,
        tags = ["manual"]):
    """Generate a JS file containing inline WASM and glue JS file.

    Prepares generated glue with module JS to generate the final wasm JS file.

    Example usage:
        inline_wasm_udf_js(
            name = "hello_udf_js",
            glue_js = "hello.js",
        )

    Args:
        name: BUILD target name
        glue_js: Javascript glue code
        tags: tags to propagate to rules
    """
    get_module_js = """async function getModule(){
            var Module = {
            instantiateWasm: function (imports, successCallback) {
                var module = new WebAssembly.Module(WasmModuleArray);
                var instance = new WebAssembly.Instance(module, imports);
                Module.testWasmInstantiationSucceeded = 1;
                successCallback(instance);
                return instance.exports;
            }
            };
            return await wasmModule(Module);
        }"""

    native.genrule(
        name = name,
        srcs = [glue_js],
        outs = ["{}_generated.js".format(name)],
        cmd_bash = """cat << EOF > $@
$$(cat $(location {glue_js}))
{module_js}
EOF""".format(
            glue_js = glue_js,
            module_js = get_module_js,
        ),
        visibility = ["//visibility:public"],
        tags = tags,
    )

def inline_wasm_udf_delta(
        name,
        wasm_binary,
        glue_js,
        custom_udf_js,
        custom_udf_js_handler = "HandleRequest",
        output_file_name = "DELTA_0000000000000005",
        logical_commit_time = None,
        wasm_as_inline_array = False,
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
            output_file_name="DELTA_0000000000000005",
            logical_commit_time="123123123"
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
        logical_commit_time: Logical commit timestamp for UDF config. Optional, defaults to now.
        wasm_as_inline_array: Whether to include WASM an inline array in the JS code snippet
            rather than passing it as a `wasm_bin` in the `UserDefinedFunctionsConfig`.
            Default False.
        udf_tool: build target for the udf_delta_file_generator.
            Defaults to `//tools/udf/udf_generator:udf_delta_file_generator`
        tags: tags to propagate to rules
    """
    inline_wasm_module_udf_js(
        name = "{}_module_wasm_js".format(name),
        glue_js = glue_js,
    )

    inline_wasm_udf_js(
        name = "{}_inline_wasm_js".format(name),
        wasm_binary = wasm_binary,
        glue_js = glue_js,
    )

    inline_wasm_js_file = "{}_inline_wasm_js_generated.js".format(name) if wasm_as_inline_array else "{}_module_wasm_js_generated.js".format(name)

    native.genrule(
        name = "{}_generated".format(name),
        srcs = [custom_udf_js, inline_wasm_js_file],
        outs = ["{}_generated.js".format(name)],
        cmd_bash = """cat "$(location {inline_wasm_js_file})" "$(location {custom_udf_js})" >"$@"
""".format(
            inline_wasm_js_file = inline_wasm_js_file,
            custom_udf_js = custom_udf_js,
        ),
        visibility = ["//visibility:private"],
        tags = tags,
    )

    logical_commit_time_args = [] if logical_commit_time == None else ["--logical_commit_time", logical_commit_time]
    wasm_binary_args = [] if wasm_as_inline_array else ["--wasm_binary_file_path", "$(location {})".format(wasm_binary)]
    run_binary(
        name = "{}_udf_delta".format(name),
        srcs = [
            "{}_generated".format(name),
            wasm_binary,
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
        ] + logical_commit_time_args + wasm_binary_args,
        tool = udf_tool,
        visibility = ["//visibility:private"],
        tags = tags,
    )

    native.genrule(
        name = name,
        srcs = [
            "{}_udf_delta".format(name),
            "{}_generated".format(name),
        ],
        outs = ["{}_copy_to_dist.bin".format(name)],
        cmd_bash = """cat << EOF > '$@'
mkdir -p dist/deltas
mkdir -p dist/udfs
cp $(location {name}_udf_delta) dist/deltas
cp $(location {name}_generated) dist/udfs
builders/tools/normalize-dist
EOF""".format(name = name),
        executable = True,
        local = True,
        message = "Copying {} dist directory".format(output_file_name),
        tags = tags,
    )

def cc_inline_wasm_udf_delta(
        name,
        srcs,
        custom_udf_js,
        custom_udf_js_handler = "HandleRequest",
        output_file_name = "DELTA_0000000000000005",
        logical_commit_time = None,
        wasm_as_inline_array = False,
        udf_tool = "//tools/udf/udf_generator:udf_delta_file_generator",
        deps = [],
        tags = ["manual"],
        linkopts = []):
    """Generate a JS + inline WASM UDF delta file and put it under dist/ directory

    Performs the following steps:
    1. Takes a C++ source file and uses emscripten to compile it to inline WASM + JS.
    2. The generated JS file is then prepended to the custom udf JS.
    3. The final JS file is used to generate a UDF delta file.

    Example usage:
        cc_inline_wasm_udf_delta(
            name = "foo_delta",
            srcs = ["foo.cc"],
            deps = [
              "//bar:foo_deps",
            ],
            custom_udf_js = "my_udf.js",
            custom_udf_js_handler = "HandleRequest",
            output_file_name = "DELTA_0000000000000005",
            logical_commit_time="123123123",
            linkopts = [
              # Allow memory growth
              "-s ALLOW_MEMORY_GROWTH=1",
              # Max memory limit of 1000MB
              "-s MAXIMUM_MEMORY=1000MB"
            ],
        )

    Args:
        name: BUILD target name
        srcs: List of C and C++ files that are processed to create the target
        custom_udf_js: Custom UDF js to be included in the final JS
        custom_udf_js_handler: Handler name of custom UDF.
        output_file_name: Name of UDF delta file output.
            Recommended to follow DELTA file naming convention.
            Defaults to `DELTA_0000000000000005`
        logical_commit_time: Logical commit timestamp for UDF config.
        wasm_as_inline_array: Whether to include WASM an inline array in the JS code snippet
            rather than passing it as a `wasm_bin` in the `UserDefinedFunctionsConfig`.
            Default False.
        udf_tool: build target for the udf_delta_file_generator.
            Defaults to `//tools/udf/udf_generator:udf_delta_file_generator`
        tags: tags to propagate to rules
        deps: List of other libraries to be linked in to the cc_binary target
        linkopts: Add these flags to the C++ linker command in addition to default
            linkopts at
            https://github.com/privacysandbox/data-plane-shared-libraries/blob/dad1d78eaffc0e74eb70090cb3a5560166d5f4c6/build_defs/cc/wasm.bzl#L18
    """

    # Generate WASM + JS using emscripten
    inline_wasm_cc_binary(
        name = "{}_inline".format(name),
        srcs = srcs,
        outputs = [
            "{}_wasm_bin.wasm".format(name),
            "{}_glue.js".format(name),
        ],
        deps = deps,
        tags = tags,
        linkopts = linkopts,
    )

    inline_wasm_udf_delta(
        name = name,
        wasm_binary = ":{}_wasm_bin.wasm".format(name),
        glue_js = ":{}_glue.js".format(name),
        custom_udf_js = custom_udf_js,
        custom_udf_js_handler = custom_udf_js_handler,
        output_file_name = output_file_name,
        logical_commit_time = logical_commit_time,
        wasm_as_inline_array = wasm_as_inline_array,
        udf_tool = udf_tool,
        tags = tags,
    )
