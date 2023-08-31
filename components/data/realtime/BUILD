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
        "//:gcp_platform": [
            "delta_file_record_change_notifier_gcp.cc",
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
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@google_privacysandbox_servers_common//src/cpp/telemetry",
    ],
)

cc_test(
    name = "delta_file_record_change_notifier_test",
    size = "small",
    srcs = select({
        "//:aws_platform": [
            "delta_file_record_change_notifier_aws_test.cc",
        ],
        "//:gcp_platform": [
            "delta_file_record_change_notifier_gcp_test.cc",
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
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:mocks",
    ],
)

cc_library(
    name = "realtime_notifier",
    srcs = select({
        # This will be uncommented once the full implementaiton
        # is added. Otherwise it breaks the gcp logic at runtime
        # "//:gcp_platform": [
        #     "realtime_notifier_gcp.cc",
        # ],
        "//conditions:default": [
            "realtime_notifier_aws.cc",
        ],
    }),
    hdrs = [
        "realtime_notifier.h",
        "realtime_notifier_metadata.h",
    ],
    deps = select({
        # "//:gcp_platform": [
        #     "//components/data/common:message_service",
        #     "@com_github_googleapis_google_cloud_cpp//:pubsub",
        # ],
        "//conditions:default": [
            ":delta_file_record_change_notifier",
            "//components/errors:retry",
            "//components/util:sleepfor",
            "//public:constants",
            "@google_privacysandbox_servers_common//src/cpp/util:duration",
        ],
    }) + [
        "//components/data/common:thread_manager",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "realtime_notifier_test",
    size = "small",
    srcs = select({
        # "//:gcp_platform": [
        #     "realtime_notifier_gcp_test.cc",
        # ],
        "//conditions:default": [
            "realtime_notifier_aws_test.cc",
        ],
    }),
    deps =
        select({
            # "//:gcp_platform": [
            #     "@com_github_googleapis_google_cloud_cpp//:pubsub_mocks",
            # ],
            "//conditions:default": [
            ],
        }) + [
            ":realtime_notifier",
            "//components/data/common:mocks",
            "//components/util:sleepfor_mock",
            "//public/data_loading:filename_utils",
            "@com_github_grpc_grpc//:grpc++",
            "@com_google_googletest//:gtest_main",
            "@google_privacysandbox_servers_common//src/cpp/telemetry:mocks",
        ],
)

cc_library(
    name = "realtime_thread_pool_manager",
    srcs = select({
        # "//:gcp_platform": [
        #     "realtime_thread_pool_manager_gcp.cc",
        # ],
        "//conditions:default": [
            "realtime_thread_pool_manager_aws.cc",
        ],
    }),
    hdrs = [
        "realtime_thread_pool_manager.h",
    ],
    deps = [
        ":realtime_notifier",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@google_privacysandbox_servers_common//src/cpp/telemetry",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
    ],
)

cc_test(
    name = "realtime_thread_pool_manager_test",
    size = "small",
    srcs = select({
        # "//:gcp_platform": [
        #     "realtime_thread_pool_manager_gcp_test.cc",
        # ],
        "//conditions:default": [
            "realtime_thread_pool_manager_aws_test.cc",
        ],
    }),
    deps = select({
        # "//:gcp_platform": [
        #     "@com_github_googleapis_google_cloud_cpp//:pubsub",
        #     "@com_github_googleapis_google_cloud_cpp//:pubsub_mocks",
        # ],
        "//conditions:default": [
        ],
    }) + [
        ":realtime_thread_pool_manager",
        "//components/data/common:mocks",
        "//components/util:sleepfor_mock",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:mocks",
    ],
)
