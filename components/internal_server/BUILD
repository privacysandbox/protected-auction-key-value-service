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
    "//tools:__subpackages__",
])

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
        ":lookup",
        ":string_padder",
        "//components/data_server/request_handler:ohttp_server_encryptor",
        "//components/query:driver",
        "//components/query:scanner",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_protobuf//:protobuf",
        "@google_privacysandbox_servers_common//src/cpp/telemetry",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
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
        ":mocks",
        "//components/data_server/cache",
        "//components/data_server/cache:key_value_cache",
        "//components/data_server/cache:mocks",
        "//public/test_util:proto_matcher",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/cpp/encryption/key_fetcher/src:fake_key_fetcher_manager",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:mocks",
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
        ":lookup",
        ":remote_lookup_client_impl",
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
    config = "//:buf.yaml",
    targets = [
        ":internal_lookup_proto",
    ],
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

cc_library(
    name = "lookup",
    hdrs = ["lookup.h"],
    deps = [
        ":internal_lookup_cc_proto",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name =
        "local_lookup",
    srcs = ["local_lookup.cc"],
    hdrs = ["local_lookup.h"],
    deps = [
        ":internal_lookup_cc_proto",
        ":lookup",
        "//components/data_server/cache",
        "//components/query:driver",
        "//components/query:scanner",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status:statusor",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
    ],
)

cc_library(
    name =
        "sharded_lookup",
    srcs = ["sharded_lookup.cc"],
    hdrs = ["sharded_lookup.h"],
    deps = [
        ":internal_lookup_cc_grpc",
        ":internal_lookup_cc_proto",
        ":local_lookup",
        ":remote_lookup_client_impl",
        "//components/query:driver",
        "//components/query:scanner",
        "//components/sharding:shard_manager",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
        "@distributed_point_functions//pir/hashing:sha256_hash_family",
        "@google_privacysandbox_servers_common//src/cpp/telemetry",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
    ],
)

cc_test(
    name = "sharded_lookup_test",
    size = "small",
    srcs = [
        "sharded_lookup_test.cc",
    ],
    deps = [
        ":internal_lookup_cc_grpc",
        ":mocks",
        ":sharded_lookup",
        "//components/data_server/cache:mocks",
        "//components/sharding:mocks",
        "//public/test_util:proto_matcher",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/cpp/encryption/key_fetcher/src:fake_key_fetcher_manager",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:mocks",
    ],
)

cc_library(
    name = "remote_lookup_client_impl",
    srcs = [
        "remote_lookup_client_impl.cc",
    ],
    hdrs = [
        "remote_lookup_client.h",
    ],
    deps = [
        ":constants",
        ":internal_lookup_cc_grpc",
        ":string_padder",
        "//components/data_server/request_handler:ohttp_client_encryptor",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@google_privacysandbox_servers_common//src/cpp/telemetry",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
    ],
)

cc_library(
    name = "string_padder",
    srcs = [
        "string_padder.cc",
    ],
    hdrs = [
        "string_padder.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_github_google_quiche//quiche:quiche_unstable_api",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "string_padder_test",
    size = "small",
    srcs = [
        "string_padder_test.cc",
    ],
    deps = [
        ":string_padder",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "remote_lookup_client_impl_test",
    size = "small",
    srcs = [
        "remote_lookup_client_impl_test.cc",
    ],
    deps = [
        ":lookup_server_impl",
        ":mocks",
        ":remote_lookup_client_impl",
        "//components/data_server/cache",
        "//components/data_server/cache:mocks",
        "//public/test_util:proto_matcher",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/cpp/encryption/key_fetcher/src:fake_key_fetcher_manager",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:mocks",
    ],
)

cc_test(
    name = "local_lookup_test",
    size = "small",
    srcs = [
        "local_lookup_test.cc",
    ],
    deps = [
        ":local_lookup",
        "//components/data_server/cache:mocks",
        "//public/test_util:proto_matcher",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:mocks",
    ],
)
