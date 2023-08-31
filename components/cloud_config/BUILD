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
    "//components:__subpackages__",
])

cc_library(
    name = "parameter_client_local",
    srcs = [
        "parameter_client_local.cc",
    ],
    hdrs = [
        "parameter_client.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:marshalling",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "parameter_client_gcp",
    srcs = [
        "parameter_client_gcp.cc",
    ],
    hdrs = [
        "parameter_client.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@control_plane_shared//cc/public/core/interface:errors",
        "@control_plane_shared//cc/public/cpio/interface/parameter_client",
    ],
)

cc_test(
    name = "parameter_client_test",
    size = "small",
    srcs = select({
        "//:aws_platform": [
            "parameter_client_aws_test.cc",
        ],
        "//:gcp_platform": [
            ":parameter_client_gcp_test.cc",
        ],
        "//:local_platform": [
            ":parameter_client_local_test.cc",
        ],
    }),
    deps = select({
        "//:aws_platform": [
            "//components/util:platform_initializer",
            "@aws_sdk_cpp//:core",
            "@aws_sdk_cpp//:ssm",
        ],
        "//:gcp_platform": [
            "@com_google_googletest//:gtest",
        ],
        "//:local_platform": [
            "//components/data/common:mocks",
            "//components/util:sleepfor_mock",
            "@com_github_google_glog//:glog",
            "@com_github_grpc_grpc//:grpc++",
        ],
    }) + [
        ":parameter_client",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "parameter_client",
    srcs = select({
        "//:aws_platform": ["parameter_client_aws.cc"],
        "//:gcp_platform": [":parameter_client_gcp"],
        "//:local_platform": [":parameter_client_local"],
    }),
    hdrs = [
        "parameter_client.h",
    ],
    deps = select({
        "//:aws_platform": [
            "//components/errors:aws_error_util",
            "@aws_sdk_cpp//:core",
            "@aws_sdk_cpp//:ssm",
        ],
        "//:gcp_platform": [
            "@control_plane_shared//cc/public/core/interface:errors",
            "@control_plane_shared//cc/public/cpio/interface:cpio",
            "@control_plane_shared//cc/public/cpio/interface/parameter_client",
        ],
        "//conditions:default": [],
    }) + [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "instance_client",
    srcs = select({
        "//:aws_instance": ["instance_client_aws.cc"],
        "//:gcp_instance": ["instance_client_gcp.cc"],
        "//:local_instance": ["local_instance_client.cc"],
    }),
    hdrs = [
        "instance_client.h",
    ],
    deps = select({
        "//:aws_instance": [
            "//components/errors:aws_error_util",
            "@aws_sdk_cpp//:autoscaling",
            "@aws_sdk_cpp//:core",
            "@aws_sdk_cpp//:ec2",
        ],
        "//:gcp_instance": [
            "@com_google_absl//absl/flags:flag",
            "@com_google_absl//absl/log:check",
            "@com_google_absl//absl/synchronization",
            "@control_plane_shared//cc/public/core/interface:execution_result",
            "@control_plane_shared//cc/public/cpio/interface/instance_client",
        ],
        "//:local_instance": [
            "@com_google_absl//absl/flags:flag",
        ],
        "//conditions:default": [],
    }) + [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
    ],
)
