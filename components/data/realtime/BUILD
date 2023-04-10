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
    "//tools:__subpackages__",
])

cc_library(
    name = "delta_file_record_change_notifier",
    srcs = select({
        "//:aws_platform": [
            "delta_file_record_change_notifier_aws.cc",
        ],
        "//:local_platform": [
            "delta_file_record_change_notifier_local.cc",
        ],
    }),
    hdrs = [
        "delta_file_record_change_notifier.h",
    ],
    deps = [
        "//components/data/common:change_notifier",
        "//components/telemetry",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "delta_file_record_change_notifier_test",
    size = "small",
    srcs = select({
        "//:aws_platform": [
            "delta_file_record_change_notifier_aws_test.cc",
        ],
        "//:local_platform": ["delta_file_record_change_notifier_local_test.cc"],
    }),
    data = [
        "//components/data/realtime/testdata:data",
    ],
    deps = [
        ":delta_file_record_change_notifier",
        "//components/data/common:change_notifier",
        "//components/data/common:mocks",
        "//components/telemetry:mocks",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "realtime_notifier",
    srcs = [
        "realtime_notifier.cc",
    ],
    hdrs = [
        "realtime_notifier.h",
    ],
    deps = [
        ":delta_file_record_change_notifier",
        "//components/data/common:thread_notifier",
        "//components/errors:retry",
        "//components/util:duration",
        "//public:constants",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "realtime_notifier_test",
    size = "small",
    srcs = [
        "realtime_notifier_test.cc",
    ],
    deps = [
        ":realtime_notifier",
        "//components/data/common:mocks",
        "//components/errors:mocks",
        "//components/telemetry:mocks",
        "//public/data_loading:filename_utils",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
    ],
)
