load(
    "//third_party/fledge_kv_server/google_internal/piper:configure_copts.bzl",
    "COPTS",
)

package(
    default_applicable_licenses = ["//third_party/fledge_kv_server:license"],
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "grpc_client",
    srcs = ["grpc_client.cc"],
    hdrs = ["grpc_client.h"],
    copts = COPTS,
    deps = [
        "//net/grpc:grpc++",
        "//net/grpc/no_envelope:grpc++_google_resolver",
        "//third_party/absl/log",
        "//third_party/absl/status",
        "//third_party/absl/status:statusor",
        "//third_party/absl/time",
        "//third_party/fledge_kv_server/google_internal/piper/public/query/v2:get_values_v2_cc_grpc",
        "//third_party/grpc:gpr",
        "//third_party/grpc:grpc++",
    ],
)

cc_test(
    name = "grpc_client_test",
    srcs = ["grpc_client_test.cc"],
    copts = COPTS,
    deps = [
        ":grpc_client",
        "//third_party/absl/status",
        "//third_party/fledge_kv_server/google_internal/piper:gtest_main",
        "//third_party/fledge_kv_server/google_internal/piper/public/query/v2:get_values_v2_cc_grpc",
        "//third_party/fledge_kv_server/public/test_util:proto_matcher",
    ],
)
