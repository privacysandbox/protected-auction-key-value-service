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

load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = [
    "//components:__subpackages__",
])

cc_library(
    name = "telemetry",
    srcs = [
        "telemetry.cc",
    ],
    hdrs = [
        "telemetry.h",
    ],
    deps = [
        ":init",
        "//components/util:version_linkstamp",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@io_opentelemetry_cpp//sdk/src/metrics",
        "@io_opentelemetry_cpp//sdk/src/resource",
        "@io_opentelemetry_cpp//sdk/src/trace",
    ],
)

cc_library(
    name = "metrics_recorder",
    srcs = [
        "metrics_recorder.cc",
    ],
    hdrs = [
        "metrics_recorder.h",
    ],
    deps = [
        ":telemetry",
        "//components/util:version_linkstamp",
        "@com_google_absl//absl/status",
    ],
)

config_setting(
    name = "local_otel_export_otlp",
    flag_values = {
        ":local_otel_export": "otlp",
    },
    visibility = [
        "//components/util:__pkg__",
    ],
)

config_setting(
    name = "local_otel_export_ostream",
    flag_values = {
        ":local_otel_export": "ostream",
    },
    visibility = [
        "//components/util:__pkg__",
    ],
)

config_setting(
    name = "local_otel_export_zipkin",
    flag_values = {
        ":local_otel_export": "zipkin",
    },
    visibility = [
        "//components/util:__pkg__",
    ],
)

string_flag(
    name = "local_otel_export",
    build_setting_default = "ostream",
    values = [
        "ostream",
        "otlp",
        "zipkin",
    ],
)

cc_library(
    name = "init_local",
    srcs = ["init_local.cc"] + select({
        ":local_otel_export_ostream": ["init_local_ostream.cc"],
        ":local_otel_export_otlp": ["init_local_otlp.cc"],
        ":local_otel_export_zipkin": ["init_local_zipkin.cc"],
    }),
    hdrs = [
        "init.h",
    ],
    deps = [
        "@com_google_absl//absl/status:statusor",
        "@io_opentelemetry_cpp//exporters/ostream:ostream_metric_exporter",
    ] + select({
        ":local_otel_export_ostream": [
            "@io_opentelemetry_cpp//exporters/ostream:ostream_span_exporter",
        ],
        ":local_otel_export_otlp": ["@io_opentelemetry_cpp//exporters/otlp:otlp_grpc_exporter"],
        ":local_otel_export_zipkin": ["@io_opentelemetry_cpp//exporters/zipkin:zipkin_exporter"],
    }),
)

cc_library(
    name = "init_aws",
    srcs = ["init_aws.cc"],
    hdrs = [
        "init.h",
    ],
    deps = [
        ":aws_xray",
        "@io_opentelemetry_cpp//exporters/otlp:otlp_grpc_exporter",
        "@io_opentelemetry_cpp//exporters/otlp:otlp_grpc_metric_exporter",
    ],
)

cc_library(
    name = "init",
    deps = select({
        "//:aws_instance": [
            ":init_aws",
        ],
        "//:local_instance": [
            ":init_local",
        ],
        "//conditions:default": [],
    }),
)

cc_library(
    name = "aws_xray",
    srcs = ["trace_generator_aws.cc"],
    hdrs = ["trace_generator_aws.h"],
    deps = [
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@io_opentelemetry_cpp//sdk/src/trace",
    ],
)

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = ["mocks.h"],
    deps = [
        ":metrics_recorder",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "aws_xray_id_generator_test",
    srcs = ["xray_id_generator_aws_test.cc"],
    deps = [
        ":aws_xray",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
