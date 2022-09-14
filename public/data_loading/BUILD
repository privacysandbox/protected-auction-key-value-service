load(
    "@com_github_google_flatbuffers//:build_defs.bzl",
    "flatbuffer_cc_library",
)
load("@rules_buf//buf:defs.bzl", "buf_lint_test")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library", "cc_test")
load("@rules_proto//proto:defs.bzl", "proto_descriptor_set", "proto_library")

package(default_visibility = ["//visibility:public"])

flatbuffer_cc_library(
    name = "data_loading_fbs",
    srcs = ["data_loading.fbs"],
    flatc_args = [
        "--force-empty",
        "--cpp-std c++17",
    ],
)

proto_library(
    name = "riegeli_metadata_proto",
    srcs = ["riegeli_metadata.proto"],
    deps = [
        "//public:base_types_proto",
        "@com_google_riegeli//riegeli/records:records_metadata_proto",
    ],
)

buf_lint_test(
    name = "riegeli_metadata_proto_lint",
    config = "//public:buf.yaml",
    targets = [":riegeli_metadata_proto"],
)

cc_library(
    name = "records_utils",
    srcs = ["records_utils.cc"],
    hdrs = ["records_utils.h"],
    deps = [
        ":data_loading_fbs",
    ],
)

proto_descriptor_set(
    name = "riegeli_metadata_descriptor_set",
    deps = [":riegeli_metadata_proto"],
)

cc_proto_library(
    name = "riegeli_metadata_cc_proto",
    deps = [":riegeli_metadata_proto"],
)

cc_library(
    name = "filename_utils",
    srcs = ["filename_utils.cc"],
    hdrs = ["filename_utils.h"],
    deps = [
        "//public:constants",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "filename_utils_test",
    srcs = [
        "filename_utils_test.cc",
    ],
    deps = [
        ":filename_utils",
        "@com_google_googletest//:gtest_main",
    ],
)
