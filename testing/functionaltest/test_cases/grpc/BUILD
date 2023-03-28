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

package(default_visibility = ["//testing:__subpackages__"])

# list of test suites, defining name, endpoint and rpc
grpc_test_suites = [
    (
        "kvserver_getvalues",
        "KVSERVER_ENDPOINT",
        "kv_server.v1.KeyValueService.GetValues",
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

csv_files = [
    ("delta_abc", "DELTA_0000000000000001", ":data/abc.csv"),
    ("delta_foo", "DELTA_0000000000000002", ":data/foo.csv"),
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

filegroup(
    name = "delta_files",
    srcs = [
        name
        for name, _, _ in csv_files
    ],
)
