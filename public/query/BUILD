load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@rules_buf//buf:defs.bzl", "buf_lint_test")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_descriptor_set", "proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "get_values_proto",
    srcs = ["get_values.proto"],
    deps = [
        "@com_google_googleapis//google/api:annotations_proto",
        "@com_google_googleapis//google/api:httpbody_proto",
        "@com_google_protobuf//:struct_proto",
    ],
)

buf_lint_test(
    name = "get_values_proto_lint",
    config = "//public:buf.yaml",
    targets = [":get_values_proto"],
)

proto_descriptor_set(
    name = "get_values_descriptor_set",
    deps = [":get_values_proto"],
)

cc_proto_library(
    name = "get_values_cc_proto",
    deps = [":get_values_proto"],
)

cc_grpc_library(
    name = "get_values_cc_grpc",
    srcs = [":get_values_proto"],
    grpc_only = True,
    deps = [":get_values_cc_proto"],
)
