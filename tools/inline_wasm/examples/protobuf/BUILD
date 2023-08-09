# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_binary")
load("//tools/inline_wasm:wasm.bzl", "inline_wasm_udf_delta")

BASE_LINKOPTS = [
    # Enable embind
    "--bind",
    # no main function
    "--no-entry",
    # optimization
    "-O3",
    "-s MODULARIZE=1",
    "-s EXPORT_NAME=wasmModule",
    "-s EXPORTED_FUNCTIONS=_malloc,_free",
    # Disable the filesystem.
    "-s FILESYSTEM=0",
]

cc_binary(
    name = "hello_cc",
    srcs = ["hello.cc"],
    linkopts = BASE_LINKOPTS,
    # This target won't build successfully on its own because of missing emscripten
    # headers etc. Therefore, we hide it from wildcards.
    tags = ["manual"],
    deps = [
        "//public/query/v2:get_values_v2_cc_proto",
    ],
)

# bazel run --config=emscripten \
# //tools/inline_wasm/examples/protobuf:hello_delta
inline_wasm_udf_delta(
    name = "hello_delta",
    cc_target = ":hello_cc",
    custom_udf_js = "my_udf.js",
    tags = ["manual"],
)
