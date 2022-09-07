load("@rules_buf//buf:defs.bzl", "buf_lint_test")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library", "cc_test")
load("@rules_proto//proto:defs.bzl", "proto_descriptor_set", "proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "base_types_proto",
    srcs = ["base_types.proto"],
    deps = [
    ],
)

buf_lint_test(
    name = "base_types_proto_lint",
    config = "//public:buf.yaml",
    targets = [":base_types_proto"],
)

proto_descriptor_set(
    name = "base_types_descriptor_set",
    deps = [":base_types_proto"],
)

cc_proto_library(
    name = "base_types_cc_proto",
    deps = [":base_types_proto"],
)

cc_library(
    name = "constants",
    srcs = ["constants.cc"],
    hdrs = ["constants.h"],
    deps = [
        "//public:base_types_cc_proto",
        "@com_google_absl//absl/strings",
    ],
)

exports_files([
    "buf.yaml",
])

cc_test(
    name = "constants_test",
    srcs = [
        "constants_test.cc",
    ],
    deps = [
        ":constants",
        "@com_google_googletest//:gtest_main",
    ],
)
