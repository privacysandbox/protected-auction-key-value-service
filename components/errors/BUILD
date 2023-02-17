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
    name = "aws_error_util",
    hdrs = [
        "error_util_aws.h",
    ],
    deps = [
        "@aws_sdk_cpp//:core",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_library(
    name = "retry",
    srcs = [
        "retry.cc",
    ],
    hdrs = [
        "retry.h",
    ],
    deps = [
        "//components/telemetry",
        "//components/telemetry:metrics_recorder",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "retry_test",
    size = "small",
    srcs = [
        "retry_test.cc",
    ],
    deps = [
        ":mocks",
        ":retry",
        "//components/telemetry:mocks",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = ["mocks.h"],
    deps = [
        ":retry",
        "@com_google_googletest//:gtest",
    ],
)
