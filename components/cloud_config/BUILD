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

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = [
    "//components:__subpackages__",
])

cc_library(
    name = "parameter_client",
    srcs = select({
        "//:aws_platform": ["aws_parameter_client.cc"],
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
        "//:aws_instance": ["aws_instance_client.cc"],
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
