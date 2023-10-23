# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library", "cc_test")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = [
    "//components/data_server:__subpackages__",
    "//components/internal_server:__subpackages__",
])

cc_library(
    name = "get_values_handler",
    srcs = [
        "get_values_handler.cc",
    ],
    hdrs = [
        "get_values_handler.h",
    ],
    deps = [
        ":get_values_adapter",
        "//components/data_server/cache",
        "//public:base_types_cc_proto",
        "//public:constants",
        "//public/query:get_values_cc_grpc",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
        "@google_privacysandbox_servers_common//src/cpp/telemetry",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
    ],
)

cc_test(
    name = "get_values_handler_test",
    size = "small",
    srcs = [
        "get_values_handler_test.cc",
    ],
    deps = [
        ":get_values_handler",
        ":mocks",
        "//components/data_server/cache",
        "//components/data_server/cache:key_value_cache",
        "//components/data_server/cache:mocks",
        "//public/query:get_values_cc_grpc",
        "//public/test_util:proto_matcher",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:mocks",
    ],
)

cc_library(
    name = "get_values_v2_handler",
    srcs = [
        "get_values_v2_handler.cc",
    ],
    hdrs = [
        "get_values_v2_handler.h",
    ],
    deps = [
        ":compression",
        ":ohttp_server_encryptor",
        "//components/data_server/cache",
        "//components/udf:udf_client",
        "//public:api_schema_cc_proto",
        "//public:base_types_cc_proto",
        "//public/query/v2:get_values_v2_cc_grpc",
        "@com_github_google_glog//:glog",
        "@com_github_google_quiche//quiche:binary_http_unstable_api",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
        "@google_privacysandbox_servers_common//src/cpp/telemetry",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
        "@nlohmann_json//:lib",
    ],
)

cc_library(
    name = "compression",
    srcs = [
        "compression.cc",
        "compression_brotli.cc",
        "uncompressed.cc",
    ],
    hdrs = [
        "compression.h",
        "compression_brotli.h",
        "uncompressed.h",
    ],
    deps = [
        "@brotli//:brotlidec",
        "@brotli//:brotlienc",
        "@com_github_google_glog//:glog",
        "@com_github_google_quiche//quiche:quiche_unstable_api",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "uncompressed_test",
    srcs = ["uncompressed_test.cc"],
    deps = [
        ":compression",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "compression_brotli_test",
    srcs = ["compression_brotli_test.cc"],
    deps = [
        ":compression",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "get_values_v2_handler_test",
    size = "small",
    srcs = [
        "get_values_v2_handler_test.cc",
    ],
    deps = [
        ":get_values_v2_handler",
        "//components/data_server/cache",
        "//components/data_server/cache:mocks",
        "//components/udf:mocks",
        "//components/udf:udf_client",
        "//public/query/v2:get_values_v2_cc_grpc",
        "//public/test_util:proto_matcher",
        "@com_github_google_glog//:glog",
        "@com_github_google_quiche//quiche:binary_http_unstable_api",
        "@com_github_google_quiche//quiche:oblivious_http_unstable_api",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/cpp/encryption/key_fetcher/src:fake_key_fetcher_manager",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:mocks",
    ],
)

cc_library(
    name = "get_values_adapter",
    srcs = [
        "get_values_adapter.cc",
    ],
    hdrs = [
        "get_values_adapter.h",
    ],
    deps = [
        ":get_values_v2_handler",
        ":v2_response_data_cc_proto",
        "//public:api_schema_cc_proto",
        "//public/applications/pa:api_overlay_cc_proto",
        "//public/applications/pa:response_utils",
        "//public/query:get_values_cc_grpc",
        "//public/query/v2:get_values_v2_cc_grpc",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_protobuf//:protobuf",
        "@google_privacysandbox_servers_common//src/cpp/util/status_macro:status_macros",
    ],
)

cc_test(
    name = "get_values_adapter_test",
    size = "small",
    srcs = [
        "get_values_adapter_test.cc",
    ],
    deps = [
        ":get_values_adapter",
        ":mocks",
        "//components/udf:mocks",
        "//public/applications/pa:api_overlay_cc_proto",
        "//public/applications/pa:response_utils",
        "//public/query:get_values_cc_grpc",
        "//public/test_util:proto_matcher",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/cpp/encryption/key_fetcher/src:fake_key_fetcher_manager",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:mocks",
        "@nlohmann_json//:lib",
    ],
)

cc_test(
    name = "v2_response_data_proto_test",
    size = "small",
    srcs = [
        "v2_response_data_proto_test.cc",
    ],
    deps = [
        ":v2_response_data_cc_proto",
        "//public/test_util:proto_matcher",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

proto_library(
    name = "v2_response_data_proto",
    srcs = ["v2_response_data.proto"],
    deps = [
        "@com_google_protobuf//:struct_proto",
    ],
)

cc_proto_library(
    name = "v2_response_data_cc_proto",
    deps = [":v2_response_data_proto"],
)

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = ["mocks.h"],
    deps = [
        ":get_values_adapter",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "ohttp_client_encryptor",
    srcs = [
        "ohttp_client_encryptor.cc",
    ],
    hdrs = [
        "ohttp_client_encryptor.h",
    ],
    copts = select({
        "//:aws_platform": ["-DCLOUD_PLATFORM_AWS=1"],
        "//:gcp_platform": ["-DCLOUD_PLATFORM_GCP=1"],
        "//conditions:default": [],
    }),
    deps = [
        "//public:constants",
        "@com_github_google_quiche//quiche:oblivious_http_unstable_api",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@google_privacysandbox_servers_common//src/cpp/encryption/key_fetcher/src:key_fetcher_manager",
    ],
)

cc_library(
    name = "ohttp_server_encryptor",
    srcs = [
        "ohttp_server_encryptor.cc",
    ],
    hdrs = [
        "ohttp_server_encryptor.h",
    ],
    deps = [
        "//public:constants",
        "@com_github_google_quiche//quiche:oblivious_http_unstable_api",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@google_privacysandbox_servers_common//src/cpp/encryption/key_fetcher/src:key_fetcher_manager",
    ],
)

cc_test(
    name = "ohttp_encryptor_test",
    size = "small",
    srcs = [
        "ohttp_encryptor_test.cc",
    ],
    deps = [
        ":ohttp_client_encryptor",
        ":ohttp_server_encryptor",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/cpp/encryption/key_fetcher/src:fake_key_fetcher_manager",
    ],
)
