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
    "//components/data_server:__subpackages__",
    "//tools:__subpackages__",
])

cc_library(
    name = "code_config",
    srcs = [
        "code_config.cc",
    ],
    hdrs = ["code_config.h"],
    deps = [
    ],
)

cc_library(
    name = "udf_client",
    srcs = [
        "udf_client.cc",
    ],
    hdrs = [
        "udf_client.h",
    ],
    deps = [
        ":code_config",
        ":get_values_hook",
        ":run_query_hook",
        "//components/errors:retry",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@control_plane_shared//cc/roma/interface:roma_interface_lib",
        "@control_plane_shared//cc/roma/roma_service/src:roma_service_lib",
    ],
)

cc_library(
    name = "noop_udf_client",
    srcs = [
        "noop_udf_client.cc",
    ],
    hdrs = [
        "noop_udf_client.h",
    ],
    visibility = ["//components/tools:__subpackages__"],
    deps = [
        ":code_config",
        ":udf_client",
        "//components/errors:retry",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "udf_config_builder",
    srcs = [
        "udf_config_builder.cc",
    ],
    hdrs = [
        "udf_config_builder.h",
    ],
    deps = [
        ":code_config",
        ":get_values_hook",
        ":logging_hook",
        ":run_query_hook",
        "@control_plane_shared//cc/roma/interface:roma_interface_lib",
        "@control_plane_shared//cc/roma/roma_service/src:roma_service_lib",
    ],
)

cc_library(
    name = "cache_get_values_hook",
    srcs = [
        "cache_get_values_hook.cc",
    ],
    hdrs = [
        "cache_get_values_hook.h",
    ],
    deps = [
        ":get_values_hook",
        "//components/data_server/cache",
        "//components/internal_server:internal_lookup_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
        "@nlohmann_json//:lib",
    ],
)

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
        "//components/internal_server:lookup_client_impl",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
        "@control_plane_shared//cc/roma/interface:roma_interface_lib",
        "@google_privacysandbox_servers_common//src/cpp/telemetry",
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
        "//components/internal_server:run_query_client_impl",
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
    name = "udf_client_test",
    size = "small",
    srcs = [
        "udf_client_test.cc",
    ],
    data = [
        "//components/test_data:add.wasm",
    ],
    deps = [
        ":code_config",
        ":mocks",
        ":udf_client",
        ":udf_config_builder",
        "//components/internal_server:mocks",
        "//components/udf:get_values_hook",
        "//components/udf:run_query_hook",
        "//public/test_util:proto_matcher",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@control_plane_shared//cc/roma/interface:roma_interface_lib",
        "@control_plane_shared//cc/roma/roma_service/src:roma_service_lib",
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
    name = "cache_get_values_hook_test",
    size = "small",
    srcs = [
        "cache_get_values_hook_test.cc",
    ],
    deps = [
        ":cache_get_values_hook",
        "//components/data_server/cache",
        "//components/data_server/cache:key_value_cache",
        "//public/test_util:proto_matcher",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
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

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = ["mocks.h"],
    deps = [
        ":code_config",
        ":udf_client",
        "//components/internal_server:lookup_client_impl",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",
        "@control_plane_shared//cc/roma/interface:roma_interface_lib",
    ],
)
