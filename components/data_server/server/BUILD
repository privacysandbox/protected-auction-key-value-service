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

load("@bazel_skylib//rules:copy_file.bzl", "copy_file")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_library(
    name = "key_value_service_impl",
    srcs = [
        "key_value_service_impl.cc",
    ],
    hdrs = [
        "key_value_service_impl.h",
    ],
    deps = [
        "//components/data_server/cache",
        "//components/data_server/request_handler:get_values_handler",
        "//components/telemetry:metrics_recorder",
        "//public/query:get_values_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_library(
    name = "parameter_fetcher",
    srcs = ["parameter_fetcher.cc"],
    hdrs = ["parameter_fetcher.h"],
    deps = [
        "//components/cloud_config:parameter_client",
        "//components/errors:retry",
        "//components/telemetry:metrics_recorder",
        "//components/util:periodic_closure",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "lifecycle_heartbeat",
    srcs = ["lifecycle_heartbeat.cc"],
    hdrs = ["lifecycle_heartbeat.h"],
    deps = [
        "//components/cloud_config:instance_client",
        "//components/data_server/server:parameter_fetcher",
        "//components/errors:retry",
        "//components/telemetry:metrics_recorder",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "lifecycle_heartbeat_test",
    size = "small",
    srcs = [
        "lifecycle_heartbeat_test.cc",
    ],
    deps = [
        ":lifecycle_heartbeat",
        "//components/telemetry:mocks",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "key_value_service_v2_impl",
    srcs = [
        "key_value_service_v2_impl.cc",
    ],
    hdrs = [
        "key_value_service_v2_impl.h",
    ],
    deps = [
        "//components/data_server/cache",
        "//components/data_server/request_handler:get_values_v2_handler",
        "//components/telemetry:metrics_recorder",
        "//public/query/v2:get_values_v2_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_binary(
    name = "server",
    srcs = ["server.cc"],
    visibility = ["//production/packaging:__subpackages__"],
    deps = [
        ":key_value_service_impl",
        ":key_value_service_v2_impl",
        ":lifecycle_heartbeat",
        ":parameter_fetcher",
        "//components/cloud_config:instance_client",
        "//components/cloud_config:parameter_client",
        "//components/data/blob_storage:blob_storage_client",
        "//components/data/blob_storage:delta_file_notifier",
        "//components/data_server/cache",
        "//components/data_server/cache:key_value_cache",
        "//components/data_server/data_loading:data_orchestrator",
        "//components/data_server/request_handler:get_values_handler",
        "//components/data_server/request_handler:get_values_v2_handler",
        "//components/errors:retry",
        "//components/telemetry",
        "//components/telemetry:init",
        "//components/telemetry:metrics_recorder",
        "//components/util:periodic_closure",
        "//components/util:version_linkstamp",
        "//public:base_types_cc_proto",
        "//public/data_loading/readers:riegeli_stream_io",
        "//public/query:get_values_cc_grpc",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_reflection",  # for grpc_cli
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/strings",
    ],
)

# copy "server"'s output as an opaque executable, simulating a binary that's
# not built from source
copy_file(
    name = "server_copy_exe",
    src = ":server",
    out = "server_copy.exe",
    is_executable = True,
)

sh_test(
    name = "server_buildinfo_test",
    size = "small",
    srcs = [":server_copy_exe"],
    args = ["--buildinfo"],
    tags = [
        "local",  # local precludes remote caching
        "smoke",
    ],
)
