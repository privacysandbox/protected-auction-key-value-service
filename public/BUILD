load("//third_party/protobuf/bazel:proto_library.bzl", "proto_library")
load("//tools/build_defs/proto/cpp:cc_proto_library.bzl", "cc_proto_library")

package(
    default_applicable_licenses = ["//third_party/fledge_kv_server:license"],
    default_visibility = ["//visibility:public"],
)

proto_library(
    name = "api_schema_proto",
    srcs = ["api_schema.proto"],
    deps = [
        "//google/protobuf:struct",
    ],
)

cc_proto_library(
    name = "api_schema_cc_proto",
    deps = [":api_schema_proto"],
)

proto_library(
    name = "base_types_proto",
    srcs = ["base_types.proto"],
)

cc_proto_library(
    name = "base_types_cc_proto",
    deps = [":base_types_proto"],
)
