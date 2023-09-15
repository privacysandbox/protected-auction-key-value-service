# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
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
    name = "get_values_hook",
    srcs = [
        "get_values_hook.cc",
    ],
    hdrs = [
        "get_values_hook.h",
    ],
    deps = [
        "//components/internal_server:internal_lookup_cc_proto",
        "//components/internal_server:local_lookup",
        "//components/internal_server:lookup",
        "//public/udf:binary_get_values_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
        "@control_plane_shared//cc/roma/interface:roma_interface_lib",
        "@google_privacysandbox_servers_common//src/cpp/telemetry",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
        "@nlohmann_json//:lib",
    ],
)

cc_library(
    name = "run_query_hook",
    srcs = [
        "run_query_hook.cc",
    ],
    hdrs = [
        "run_query_hook.h",
    ],
    deps = [
        "//components/internal_server:internal_lookup_cc_proto",
        "//components/internal_server:lookup",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@control_plane_shared//cc/roma/interface:roma_function_binding_io_cc_proto",
        "@control_plane_shared//cc/roma/interface:roma_interface_lib",
        "@nlohmann_json//:lib",
    ],
)

cc_library(
    name = "logging_hook",
    srcs = [
        "logging_hook.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
    ],
)

cc_test(
    name = "get_values_hook_test",
    size = "small",
    srcs = [
        "get_values_hook_test.cc",
    ],
    deps = [
        ":get_values_hook",
        "//components/internal_server:mocks",
        "//public/test_util:proto_matcher",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:telemetry_provider",
    ],
)

cc_test(
    name = "run_query_hook_test",
    size = "small",
    srcs = [
        "run_query_hook_test.cc",
    ],
    deps = [
        ":run_query_hook",
        "//components/internal_server:mocks",
        "//public/test_util:proto_matcher",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
