load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "delta_record_writer",
    hdrs = ["delta_record_writer.h"],
    deps = [
        "//public/data_loading:records_utils",
        "//public/data_loading:riegeli_metadata_cc_proto",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "delta_record_stream_writer",
    srcs = ["delta_record_stream_writer.cc"],
    hdrs = ["delta_record_stream_writer.h"],
    deps = [
        ":delta_record_writer",
        "//public/data_loading:records_utils",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_riegeli//riegeli/bytes:ostream_writer",
        "@com_google_riegeli//riegeli/records:record_writer",
    ],
)

cc_test(
    name = "delta_record_stream_writer_test",
    size = "small",
    srcs = ["delta_record_stream_writer_test.cc"],
    deps = [
        ":delta_record_stream_writer",
        "//public/data_loading/readers:riegeli_stream_io",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)
