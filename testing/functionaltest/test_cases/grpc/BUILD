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
load(
    "@google_privacysandbox_functionaltest_system//bazel:rpc_test.bzl",
    "grpc_diff_test_suite",
)
load("@rules_pkg//pkg:zip.bzl", "pkg_zip")

package(
    default_visibility = [
        "//production/packaging:__subpackages__",
        "//testing:__subpackages__",
    ],
)

# list of test suites, defining name, endpoint and rpc
grpc_test_suites = [
    (
        "kvserver_getvalues_v1",
        "KVSERVER_ENDPOINT",
        "kv_server.v1.KeyValueService.GetValues",
    ),
    (
        "kvserver_getvalues_v2",
        "KVSERVER_ENDPOINT",
        "kv_server.v2.KeyValueService.GetValues",
    ),
]

[
    grpc_diff_test_suite(
        name = "grpcurl-{}".format(test_suite),
        endpoint = endpoint,
        protoset = "//public/query:query_api_descriptor_set",
        rpc = rpc,
        test_files_glob_spec = "{}/{}/**/*".format(endpoint, rpc),
        test_tags = [
            "external",
            "manual",
        ],
    )
    for test_suite, endpoint, rpc in grpc_test_suites
]

test_suite(
    name = "grpcurl_test_suite",
    tags = [
        "manual",
    ],
    tests = ["grpcurl-{}".format(test_suite) for test_suite, _, _ in grpc_test_suites],
)

csv_files = [
    (
        "delta_csv_{}".format(idx),
        delta_file.rsplit("/", 1)[1].rsplit(".")[0],
        ":{}".format(delta_file),
    )
    for idx, delta_file in enumerate(glob(["data/DELTA*.csv"]))
]

js_files = [
    (
        "delta_js_{}".format(idx),
        delta_file.rsplit("/", 1)[1].rsplit(".")[0],
        ":{}".format(delta_file),
    )
    for idx, delta_file in enumerate(glob(["data/DELTA*.js"]))
]

[
    run_binary(
        name = name,
        srcs = [
            infile,
        ],
        outs = [
            outfile,
        ],
        args = [
            "format_data",
            "--input_file",
            "$(location {})".format(infile),
            "--output_file",
            "$(location {})".format(outfile),
        ],
        tool = "//tools/data_cli",
    )
    for name, outfile, infile in csv_files
]

[
    run_binary(
        name = name,
        srcs = [
            infile,
        ],
        outs = [
            outfile,
        ],
        args = [
            "--udf_file_path",
            "$(location {})".format(infile),
            "--output_path",
            "$(location {})".format(outfile),
        ],
        tool = "//tools/udf/udf_generator:udf_delta_file_generator",
    )
    for name, outfile, infile in js_files
]

filegroup(
    name = "delta_files_filegroup",
    srcs = [
        name
        for name, _, _ in csv_files + js_files
    ],
)

pkg_zip(
    name = "delta_files",
    srcs = [":delta_files_filegroup"],
)
