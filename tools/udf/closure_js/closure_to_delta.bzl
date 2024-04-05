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
load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_binary")

def closure_to_delta(
        name,
        closure_js_library_target,
        custom_udf_js_handler = "HandleRequest",
        output_file_name = "DELTA_0000000000000009",
        logical_commit_time = None,  # Not passing a logical_commit_time will default to now.
        udf_tool = "//tools/udf/udf_generator:udf_delta_file_generator",
        tags = ["manual"]):
    """Generate a JS UDF delta file from a given closure_js_library target and put it under dist/

    Example usage:
        closure_to_delta(
            name = "my_js_delta",
            closure_js_library_target = ":my_js_lib",
            custom_udf_js_handler = "MyHandlerName",
            output_file_name = "DELTA_0000000000000009",
            logical_commit_time="123123123",
        )

    Args:
        name: BUILD target name
        closure_js_library_target: Name of the closure_js_library target
        custom_udf_js_handler: Handler (entry point) function name of custom UDF
        output_file_name: Name of UDF delta file output
        udf_tool: BUILD target for the udf_delta_file_generator.
            Defaults to `//tools/udf/udf_generator:udf_delta_file_generator`
        logical_commit_time: Logical commit timestamp for UDF config. Optional, defaults to now.
        tags: tags to propagate to rules
    """
    closure_js_binary(
        name = name + "_js_bin",
        deps = [
            closure_js_library_target,
        ],
        visibility = ["//visibility:private"],
        tags = tags,
    )

    logical_commit_time_args = [] if logical_commit_time == None else ["--logical_commit_time", logical_commit_time]

    run_binary(
        name = "{}_udf_delta".format(name),
        srcs = [
            "{}_js_bin.js".format(name),
        ],
        outs = [
            output_file_name,
        ],
        args = [
            "--udf_file_path",
            "$(location {}_js_bin.js)".format(name),
            "--output_path",
            "$(location {})".format(output_file_name),
            "--udf_handler_name",
            custom_udf_js_handler,
        ] + logical_commit_time_args,
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
mkdir -p dist/deltas
cp $(location {}_udf_delta) dist/deltas
builders/tools/normalize-dist
EOF""".format(name),
        executable = True,
        local = True,
        message = "Copying {} dist directory".format(output_file_name),
        tags = tags,
    )
