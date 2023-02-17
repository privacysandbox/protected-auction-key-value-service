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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = [
    "//components/data_server:__subpackages__",
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
        "//components/data_server/cache",
        "//components/telemetry:metrics_recorder",
        "//infrastructure/communication:bhttp_utils",
        "//public:base_types_cc_proto",
        "//public:constants",
        "//public/query:get_values_cc_grpc",
        "@com_github_google_glog//:glog",
        "@com_github_google_quiche//quiche:binary_http_unstable_api",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
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
        "//components/data_server/cache",
        "//components/data_server/cache:key_value_cache",
        "//components/data_server/cache:mocks",
        "//components/telemetry:metrics_recorder",
        "//components/telemetry:mocks",
        "//infrastructure/communication:bhttp_utils",
        "//public/query:get_values_cc_grpc",
        "//public/test_util:proto_matcher",
        "@com_github_google_quiche//quiche:binary_http_unstable_api",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
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
        "//components/data_server/cache",
        "//components/telemetry:metrics_recorder",
        "//public:base_types_cc_proto",
        "//public:constants",
        "//public/query/v2:get_values_v2_cc_grpc",
        "@com_github_google_glog//:glog",
        "@com_github_google_quiche//quiche:binary_http_unstable_api",
        "@com_github_google_quiche//quiche:oblivious_http_unstable_api",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
        "@nlohmann_json//:lib",
    ],
)

cc_library(
    name = "compression",
    srcs = ["compression.cc"],
    hdrs = ["compression.h"],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_github_google_quiche//quiche:quiche_unstable_api",
    ],
)

cc_test(
    name = "compression_test",
    srcs = ["compression_test.cc"],
    deps = [
        ":compression",
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
        "//components/telemetry:metrics_recorder",
        "//components/telemetry:mocks",
        "//infrastructure/communication:bhttp_utils",
        "//public:constants",
        "//public/query/v2:get_values_v2_cc_grpc",
        "//public/test_util:proto_matcher",
        "@com_github_google_glog//:glog",
        "@com_github_google_quiche//quiche:binary_http_unstable_api",
        "@com_github_google_quiche//quiche:oblivious_http_unstable_api",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
    ],
)
