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
        "//public/query:get_values_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
    ],
)

cc_library(
    name = "parameter_fetcher",
    srcs = select({
        "//:aws_platform": ["parameter_fetcher_aws.cc"],
        # todo: uncomment this once the GCP blob storage logic is implemented
        #"//:gcp_platform": ["parameter_fetcher_gcp.cc"],
        "//:gcp_platform": ["parameter_fetcher_local.cc"],
        "//:local_platform": ["parameter_fetcher_local.cc"],
    }) + ["parameter_fetcher.cc"],
    hdrs = ["parameter_fetcher.h"],
    deps = [
        "//components/cloud_config:parameter_client",
        "//components/data/blob_storage:blob_storage_change_notifier",
        "//components/errors:retry",
        "//components/util:periodic_closure",
        "//public:constants",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
    ],
)

cc_test(
    name = "parameter_fetcher_test",
    size = "small",
    srcs = select({
        "//:aws_platform": ["parameter_fetcher_aws_test.cc"],
        "//:gcp_platform": ["parameter_fetcher_gcp_test.cc"],
        "//:local_platform": ["parameter_fetcher_local_test.cc"],
    }),
    deps = [
        ":parameter_fetcher",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:mocks",
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
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
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
        ":mocks",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:mocks",
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
        "//public/query/v2:get_values_v2_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
    ],
)

cc_library(
    name = "server_lib",
    srcs = ["server.cc"],
    hdrs = ["server.h"],
    deps = [
        ":key_fetcher_factory",
        ":key_value_service_impl",
        ":key_value_service_v2_impl",
        ":lifecycle_heartbeat",
        ":parameter_fetcher",
        "//components/cloud_config:instance_client",
        "//components/cloud_config:parameter_client",
        "//components/data/blob_storage:blob_storage_client",
        "//components/data/blob_storage:delta_file_notifier",
        "//components/data/realtime:realtime_thread_pool_manager",
        "//components/data_server/cache",
        "//components/data_server/cache:key_value_cache",
        "//components/data_server/data_loading:data_orchestrator",
        "//components/data_server/request_handler:get_values_adapter",
        "//components/data_server/request_handler:get_values_handler",
        "//components/data_server/request_handler:get_values_v2_handler",
        "//components/errors:retry",
        "//components/internal_server:constants",
        "//components/internal_server:local_lookup",
        "//components/internal_server:lookup",
        "//components/internal_server:lookup_server_impl",
        "//components/internal_server:lookup_supplier",
        "//components/internal_server:sharded_lookup",
        "//components/sharding:cluster_mappings_manager",
        "//components/telemetry:kv_telemetry",
        "//components/udf:udf_client",
        "//components/udf:udf_config_builder",
        "//components/udf/hooks:get_values_hook",
        "//components/util:periodic_closure",
        "//components/util:platform_initializer",
        "//components/util:version_linkstamp",
        "//public:base_types_cc_proto",
        "//public:constants",
        "//public/data_loading/readers:riegeli_stream_io",
        "//public/query:get_values_cc_grpc",
        "//public/udf:constants",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_reflection",  # for grpc_cli
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/strings",
        "@google_privacysandbox_servers_common//src/cpp/telemetry",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:init",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:metrics_recorder",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:telemetry_provider",
    ],
)

cc_test(
    name = "server_lib_test",
    size = "small",
    srcs = select({
        "//:aws_platform": ["server_aws_test.cc"],
        "//:gcp_platform": ["server_gcp_test.cc"],
        "//:local_platform": ["server_local_test.cc"],
    }),
    deps = [
        ":mocks",
        ":server_lib",
        "//components/udf:mocks",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/cpp/telemetry:mocks",
    ],
)

cc_binary(
    name = "server",
    srcs = ["main.cc"],
    visibility = [
        "//google_internal/production/packaging:__subpackages__",
        "//production/packaging:__subpackages__",
    ],
    deps = [
        ":key_fetcher_factory",
        ":server_lib",
        "//components/sharding:shard_manager",
        "//components/util:version_linkstamp",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/debugging:failure_signal_handler",
        "@com_google_absl//absl/debugging:symbolize",
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

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = ["mocks.h"],
    visibility = ["//components/sharding:__subpackages__"],
    deps = [
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "key_fetcher_factory",
    srcs = [
        "key_fetcher_factory.cc",
    ],
    hdrs = [
        "key_fetcher_factory.h",
    ],
    copts =
        select(
            {
                "@google_privacysandbox_servers_common//:aws_platform": [
                    "-DCLOUD_PLATFORM_AWS=1",
                ],
                "@google_privacysandbox_servers_common//:gcp_platform": [
                    "-DCLOUD_PLATFORM_GCP=1",
                ],
            },
        ),
    deps = [
        ":parameter_fetcher",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@google_privacysandbox_servers_common//src/cpp/concurrent:executor",
        "@google_privacysandbox_servers_common//src/cpp/encryption/key_fetcher/src:fake_key_fetcher_manager",
        "@google_privacysandbox_servers_common//src/cpp/encryption/key_fetcher/src:key_fetcher_manager",
        "@google_privacysandbox_servers_common//src/cpp/encryption/key_fetcher/src:private_key_fetcher",
        "@google_privacysandbox_servers_common//src/cpp/encryption/key_fetcher/src:public_key_fetcher",
    ],
)
