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

cc_library(
    name = "request_generation_util",
    srcs = ["request_generation_util.cc"],
    hdrs = ["request_generation_util.h"],
    deps = [
        "//tools/request_simulation/request:raw_request_cc_proto",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "message_queue",
    srcs = ["message_queue.cc"],
    hdrs = ["message_queue.h"],
    deps = [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "rate_limiter",
    srcs = ["rate_limiter.cc"],
    hdrs = ["rate_limiter.h"],
    deps = [
        "//components/util:sleepfor",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@google_privacysandbox_servers_common//src/cpp/util:duration",
    ],
)

cc_library(
    name = "synthetic_request_generator",
    srcs = ["synthetic_request_generator.cc"],
    hdrs = ["synthetic_request_generator.h"],
    deps = [
        ":message_queue",
        ":rate_limiter",
        "//components/data/common:thread_manager",
        "@com_google_absl//absl/functional:any_invocable",
    ],
)

cc_library(
    name = "grpc_client",
    hdrs = ["grpc_client.h"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "request_generation_util_test",
    size = "small",
    srcs = ["request_generation_util_test.cc"],
    deps = [
        ":request_generation_util",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "message_queue_test",
    size = "small",
    srcs = ["message_queue_test.cc"],
    deps = [
        ":message_queue",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "rate_limiter_test",
    size = "small",
    srcs = ["rate_limiter_test.cc"],
    deps = [
        ":rate_limiter",
        "//components/util:sleepfor_mock",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "synthetic_request_generator_test",
    size = "small",
    srcs = ["synthetic_request_generator_test.cc"],
    deps = [
        ":request_generation_util",
        ":synthetic_request_generator",
        "//components/util:sleepfor_mock",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "grpc_client_test",
    size = "small",
    srcs = ["grpc_client_test.cc"],
    deps = [
        ":grpc_client",
        "//public/testing:fake_key_value_service_impl",
        "//tools/request_simulation/request:raw_request_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)
