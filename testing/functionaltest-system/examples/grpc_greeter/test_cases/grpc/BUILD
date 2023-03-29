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

load(
    "@google_privacysandbox_functionaltest_system//bazel:rpc_test.bzl",
    "grpc_diff_test_suite",
    "grpc_perf_test_suite",
)

rpcs = [
    ("greeter_tests", "GREETER_ENDPOINT", "helloworld.Greeter.SayHello", ":helloworld_descriptor_set.pb"),
]

[
    grpc_diff_test_suite(
        name = "grpcurl-{}".format(test_suite_name),
        endpoint = endpoint,
        protoset = protoset,
        rpc = rpc,
        test_files_glob_spec = "{}/{}/**/*".format(endpoint, rpc),
        test_tags = [
            "external",
            "manual",
        ],
    )
    for test_suite_name, endpoint, rpc, protoset in rpcs
]

[
    grpc_perf_test_suite(
        name = "ghz-{}".format(test_suite_name),
        endpoint = endpoint,
        protoset = protoset,
        rpc = rpc,
        test_files_glob_spec = "{}/{}/**/*".format(endpoint, rpc),
        test_tags = [
            "exclusive",
            "external",
            "manual",
        ],
    )
    for test_suite_name, endpoint, rpc, protoset in rpcs
]
