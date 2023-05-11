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

load("@bazel_skylib//lib:selects.bzl", "selects")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = [
    "//components:__subpackages__",
])

cc_library(
    name = "periodic_closure",
    srcs = [
        "periodic_closure.cc",
    ],
    hdrs = ["periodic_closure.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "periodic_closure_test",
    size = "small",
    srcs = ["periodic_closure_test.cc"],
    deps = [
        ":periodic_closure",
        "@com_google_googletest//:gtest_main",
    ],
)

selects.config_setting_group(
    name = "local_otel_otlp",
    match_all = [
        "//:local_instance",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:local_otel_export_otlp",
    ],
)

selects.config_setting_group(
    name = "local_otel_ostream",
    match_all = [
        "//:local_instance",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:local_otel_export_ostream",
    ],
)

local_defines = select({
    "//:aws_instance": ["INSTANCE=\\\"aws\\\""],
    "//:local_instance": ["INSTANCE=\\\"local\\\""],
    "//conditions:default": ["INSTANCE=\\\"unknown\\\""],
}) + select({
    "//:aws_platform": ["PLATFORM=\\\"aws\\\""],
    "//:local_platform": ["PLATFORM=\\\"local\\\""],
    "//conditions:default": ["PLATFORM=\\\"unknown\\\""],
}) + select({
    "//:aws_instance": ["OTEL_EXPORT=\\\"xray\\\""],
    ":local_otel_ostream": ["OTEL_EXPORT=\\\"ostream\\\""],
    ":local_otel_otlp": ["OTEL_EXPORT=\\\"otlp\\\""],
    "//conditions:default": ["OTEL_EXPORT=\\\"unknown\\\""],
})

genrule(
    name = "generate_version_linkstamp",
    outs = ["version_linkstamp.cc"],
    cmd = """
        $(execpath :generate_version_h) "$(TARGET_CPU)-$(COMPILATION_MODE)" >>$@
    """,
    # Undocumented attr to depend on workspace status files.
    # Used here because generate_version_h depends on the workspace status files.
    stamp = 1,
    tools = [":generate_version_h"],
    visibility = ["//visibility:private"],
)

cc_library(
    name = "build_flags",
    srcs = [
        "build_flags.cc",
        "build_info.cc",
    ],
    hdrs = [
        "build_info.h",
    ],
    linkstamp = ":generate_version_linkstamp",
    local_defines = local_defines,
    visibility = ["//visibility:private"],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "platform_initializer",
    srcs = select({
        "//:aws_platform": [
            "platform_initializer_aws.cc",
        ],
        "//:local_platform": [
            "platform_initializer_local.cc",
        ],
    }),
    hdrs = [
        "platform_initializer.h",
    ],
    visibility = [
        "//components:__subpackages__",
        "//tools/data_cli:__subpackages__",
    ],
    deps = select({
        "//:aws_platform": [
            "//components/errors:aws_error_util",
            "@aws_sdk_cpp//:core",
        ],
        "//conditions:default": [],
    }) + [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "version_linkstamp",
    # Linking this library makes build cache inefficient,
    visibility = [
        "//components/data_server/server:__pkg__",
        "//components/telemetry:__pkg__",
    ],
    deps = [":build_flags"],
    alwayslink = 1,
)

cc_library(
    name = "sleepfor",
    srcs = [
        "sleepfor.cc",
    ],
    hdrs = [
        "sleepfor.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "sleepfor_test",
    size = "small",
    srcs = [
        "sleepfor_test.cc",
    ],
    deps = [
        ":sleepfor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "sleepfor_mock",
    testonly = 1,
    hdrs = ["sleepfor_mock.h"],
    deps = [
        ":sleepfor",
        "@com_google_googletest//:gtest",
    ],
)
