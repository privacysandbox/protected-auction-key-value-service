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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")
load("//tools/udf/inline_wasm:wasm.bzl", "cc_inline_wasm_udf_delta")

package(default_visibility = [
    "//visibility:public",
])

BASE_LINKOPTS = [
    # Enable embind
    "--bind",
    # no main function
    "--no-entry",
    # optimization
    "-O3",
    # Do not use closure. We probably want to use closure eventually.
    "--closure=0",
    "-s MODULARIZE=1",
    "-s EXPORT_NAME=wasmModule",
    # Disable the filesystem.
    "-s FILESYSTEM=0",
    # Use environment with fewer "extra" features.
    "-s ENVIRONMENT=shell",
]

cc_binary(
    name = "handle_request_cc",
    srcs = ["handle_request.cc"],
    linkopts = BASE_LINKOPTS,
    # This target won't build successfully on its own because of missing emscripten
    # headers etc. Therefore, we hide it from wildcards.
    tags = ["manual"],
    deps = [
        ":stored_value_cc_proto",
        "//public/udf:binary_get_values_cc_proto",
        "@com_google_absl//absl/status:statusor",
        "@nlohmann_json//:lib",
    ],
)

# builders/tools/bazel-debian run --config=emscripten \
# //tools/udf/inline_wasm/examples/get_values_binary_proto:handle_request_cc_delta
cc_inline_wasm_udf_delta(
    name = "handle_request_cc_delta",
    cc_target = ":handle_request_cc",
    custom_udf_js = "my_udf.js",
    tags = ["manual"],
)

proto_library(
    name = "stored_value_proto",
    srcs = ["stored_value.proto"],
    deps = [
    ],
)

cc_proto_library(
    name = "stored_value_cc_proto",
    deps = [":stored_value_proto"],
)
