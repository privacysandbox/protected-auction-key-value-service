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

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@rules_buf//buf:defs.bzl", "buf_lint_test")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library", "cc_test")
load("@rules_proto//proto:defs.bzl", "proto_descriptor_set", "proto_library")

package(default_visibility = [
    "//components:__subpackages__",
])

cc_library(
    name = "lookup_client_impl",
    srcs = [
        "lookup_client_impl.cc",
    ],
    hdrs = [
        "lookup_client.h",
    ],
    deps = [
        ":constants",
        ":internal_lookup_cc_grpc",
        "//components/telemetry",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "lookup_server_impl",
    srcs = [
        "lookup_server_impl.cc",
    ],
    hdrs = [
        "lookup_server_impl.h",
    ],
    deps = [
        ":internal_lookup_cc_grpc",
        "//components/data_server/cache",
        "//components/telemetry",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "lookup_server_impl_test",
    size = "small",
    srcs = [
        "lookup_server_impl_test.cc",
    ],
    deps = [
        ":internal_lookup_cc_grpc",
        ":lookup_server_impl",
        "//components/data_server/cache",
        "//components/data_server/cache:mocks",
        "//public/test_util:proto_matcher",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "constants",
    hdrs = [
        "constants.h",
    ],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = ["mocks.h"],
    deps = [
        ":internal_lookup_cc_proto",
        ":lookup_client_impl",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest",
    ],
)

proto_library(
    name = "internal_lookup_proto",
    srcs = ["lookup.proto"],
    deps = [
        "@com_google_googleapis//google/rpc:status_proto",
    ],
)

buf_lint_test(
    name = "internal_lookup_lint",
    config = "//public:buf.yaml",
    targets = [":internal_lookup_proto"],
)

proto_descriptor_set(
    name = "internal_lookup_descriptor_set",
    deps = [":internal_lookup_proto"],
)

cc_proto_library(
    name = "internal_lookup_cc_proto",
    deps = [":internal_lookup_proto"],
)

cc_grpc_library(
    name = "internal_lookup_cc_grpc",
    srcs = [":internal_lookup_proto"],
    grpc_only = True,
    deps = [":internal_lookup_cc_proto"],
)
