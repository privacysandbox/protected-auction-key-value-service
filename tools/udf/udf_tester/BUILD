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

load("@bazel_skylib//rules:run_binary.bzl", "run_binary")
load("@rules_cc//cc:defs.bzl", "cc_binary")

package(default_visibility = [
    "//production/packaging/tools:__subpackages__",
    "//tools:__subpackages__",
])

cc_binary(
    name = "udf_delta_file_tester",
    srcs = ["udf_delta_file_tester.cc"],
    deps = [
        "//components/data_server/cache",
        "//components/data_server/cache:key_value_cache",
        "//components/internal_server:local_lookup",
        "//components/udf:udf_client",
        "//components/udf:udf_config_builder",
        "//components/udf/hooks:get_values_hook",
        "//public/data_loading:data_loading_fbs",
        "//public/data_loading/readers:delta_record_stream_reader",
        "//public/query/v2:get_values_v2_cc_proto",
        "//public/udf:constants",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/strings",
        "@google_privacysandbox_servers_common//src/cpp/util/status_macro:status_macros",
    ],
)

run_binary(
    name = "udf_delta",
    srcs = [
        ":example_udf.js",
    ],
    outs = [
        "DELTA_0000000000000001",
    ],
    args = [
        "--udf_file_path",
        "$(location :example_udf.js)",
        "--output_path",
        "$(location DELTA_0000000000000001)",
    ],
    tool = "//tools/udf/udf_generator:udf_delta_file_generator",
)

run_binary(
    name = "generate_data_delta",
    srcs = [
        ":example_data.csv",
    ],
    outs = [
        "DELTA_0000000000000002",
    ],
    args = [
        "format_data",
        "--input_file",
        "$(location example_data.csv)",
        "--input_format",
        "CSV",
        "--output_file",
        "$(location DELTA_0000000000000002)",
        "--output_format",
        "DELTA",
    ],
    tags = ["manual"],
    tool = "//tools/data_cli",
)
