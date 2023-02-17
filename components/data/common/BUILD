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
    "//tools:__subpackages__",
])

cc_library(
    name = "change_notifier",
    srcs = select({
        "//:aws_platform": [
            "msg_svc_aws.cc",
            "msg_svc_aws.h",
            "change_notifier_aws.cc",
        ],
        "//conditions:default": [],
    }),
    hdrs = [
        "change_notifier.h",
    ],
    deps = select({
        "//:aws_platform": [
            "@aws_sdk_cpp//:sns",
            "@aws_sdk_cpp//:sqs",
            "//components/errors:aws_error_util",
        ],
        "//conditions:default": [],
    }) + [
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_github_google_glog//:glog",
    ],
)

cc_library(
    name = "thread_notifier",
    srcs = [
        "thread_notifier.cc",
    ],
    hdrs = [
        "thread_notifier.h",
    ],
    deps = [
        "//components/errors:retry",
        "//components/util:duration",
        "//public:constants",
        "//public/data_loading:filename_utils",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = ["mocks.h"],
    deps = [
        "//components/data/blob_storage:blob_storage_client",
        "//components/data/blob_storage:delta_file_notifier",
        "//components/data/realtime:delta_file_record_change_notifier",
        "//components/data/realtime:realtime_notifier",
        "@com_google_googletest//:gtest",
    ],
)
