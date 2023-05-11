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

cc_test(
    name = "parameter_client_local_test",
    size = "small",
    srcs = ["parameter_client_local_test.cc"],
    deps = [
        ":parameter_client_local",
        "//components/data/common:mocks",
        "//components/util:sleepfor_mock",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "parameter_client",
    srcs = select({
        "//:aws_platform": ["parameter_client_aws.cc"],
        "//:local_platform": [":parameter_client_local"],
    }),
    hdrs = [
        "parameter_client.h",
    ],
    deps = select({
        "//:aws_platform": [
            "@aws_sdk_cpp//:core",
            "@aws_sdk_cpp//:ssm",
            "//components/errors:aws_error_util",
        ],
        "//conditions:default": [],
    }) + [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/status",
        "@com_github_google_glog//:glog",
    ],
)

cc_library(
    name = "instance_client",
    srcs = select({
        "//:aws_instance": ["instance_client_aws.cc"],
        "//:local_instance": ["local_instance_client.cc"],
    }),
    hdrs = [
        "instance_client.h",
    ],
    deps = select({
        "//:aws_instance": [
            "@aws_sdk_cpp//:autoscaling",
            "@aws_sdk_cpp//:core",
            "@aws_sdk_cpp//:ec2",
            "//components/errors:aws_error_util",
        ],
        "//:local_instance": [
            "@com_google_absl//absl/flags:flag",
        ],
        "//conditions:default": [],
    }) + [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/status",
        "@com_github_google_glog//:glog",
    ],
)
