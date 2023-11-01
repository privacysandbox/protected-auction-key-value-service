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
)
load(
    "@google_privacysandbox_functionaltest_system//sut:endpoint.bzl",
    "endpoint",
)
load(
    "//testing/functionaltest:test_data.bzl",
    "data_targets",
)

package(
    default_visibility = [
        "//production/packaging:__subpackages__",
        "//testing:__subpackages__",
    ],
)

_SUT_NAME = "baseline-ssp"

exposed_endpoints = {
    "FRONTEND_ENVOY_ENDPOINT": endpoint(
        docker_network = "{}-frontend".format(_SUT_NAME),
        endpoint_type = "grpc",
        host = "frontend-envoy",
        port = 3000,
    ),
    "KVSERVER_ENDPOINT": endpoint(
        docker_network = "{}-kvserver".format(_SUT_NAME),
        endpoint_type = "grpc",
        host = "kv-server",
        port = 2000,
    ),
    "KVSERVER_ENVOY_ENDPOINT": endpoint(
        docker_network = "{}-kvserver".format(_SUT_NAME),
        endpoint_type = "grpc",
        host = "kv-server-envoy",
        port = 2002,
    ),
}

# list of test suites, defining name, endpoint and rpc
grpc_test_suites = [
    (
        # test suite name
        "{}-{}".format(
            test_suite,
            endpoint_name.lower(),
        ),
        endpoint_name,
        endpoint,
        # protoset
        "//public/query:query_api_descriptor_set",
        rpc,
    )
    for test_suite, rpc in [
        ("kvserver_getvalues_v1", "kv_server.v1.KeyValueService.GetValues"),
        ("kvserver_getvalues_v2", "kv_server.v2.KeyValueService.GetValuesHttp"),
    ]
    for endpoint_name, endpoint in exposed_endpoints.items()
    if endpoint.endpoint_type == "grpc"
]

[
    rpc_diff_test_suite(
        name = "grpcurl-{}".format(test_suite),
        endpoint = endpoint,
        protoset = protoset,
        rpc = rpc,
        test_files_glob_spec = "test_cases/{}/{}/**/*".format(endpoint_name, rpc),
        test_tags = [
            "external",
            "manual",
        ],
    )
    for test_suite, endpoint_name, endpoint, protoset, rpc in grpc_test_suites
]

test_suite(
    name = "grpcurl_test_suite",
    tags = [
        "manual",
    ],
    tests = ["grpcurl-{}".format(test_suite) for test_suite, _, _, _, _ in grpc_test_suites],
)

data_targets("data", "configs")
