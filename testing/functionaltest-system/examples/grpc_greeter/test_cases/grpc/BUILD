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
    "rpc_diff_test_suite",
    "rpc_perf_test_suite",
)
load(
    "@google_privacysandbox_functionaltest_system//sut:endpoint.bzl",
    "endpoint",
)

greeter_endpoint = endpoint(
    docker_network = "greeter-example",
    endpoint_type = "grpc",
    host = "greeter",
    port = 50051,
    protoset = ":helloworld_descriptor_set.pb",
    rpc = "helloworld.Greeter.SayHello",
)

rpc_diff_test_suite(
    name = "grpcurl-greeter_tests",
    endpoint = greeter_endpoint,
    protoset = greeter_endpoint.protoset,
    rpc = greeter_endpoint.rpc,
    test_files_glob_spec = "GREETER_ENDPOINT/{}/**/*".format(greeter_endpoint.rpc),
    test_tags = [
        "external",
        "manual",
    ],
)

rpc_perf_test_suite(
    name = "ghz-greeter_tests",
    endpoint = greeter_endpoint,
    protoset = greeter_endpoint.protoset,
    rpc = greeter_endpoint.rpc,
    test_files_glob_spec = "GREETER_ENDPOINT/{}/**/*".format(greeter_endpoint.rpc),
    test_tags = [
        "exclusive",
        "external",
        "manual",
    ],
)
