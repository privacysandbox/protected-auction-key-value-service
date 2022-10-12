load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "delta_record_reader",
    hdrs = ["delta_record_reader.h"],
    deps = [
        "//public/data_loading:records_utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "riegeli_stream_io",
    hdrs = ["riegeli_stream_io.h"],
    deps = [
        "//public/data_loading:riegeli_metadata_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_riegeli//riegeli/bytes:istream_reader",
        "@com_google_riegeli//riegeli/records:record_reader",
    ],
)

cc_test(
    name = "riegeli_stream_io_test",
    srcs = [
        "riegeli_stream_io_test.cc",
    ],
    deps = [
        ":riegeli_stream_io",
        "//public/test_util:mocks",
        "//public/test_util:proto_matcher",
        "@com_google_googletest//:gtest_main",
        "@com_google_riegeli//riegeli/bytes:string_writer",
        "@com_google_riegeli//riegeli/records:record_writer",
    ],
)

cc_library(
    name = "delta_record_stream_reader",
    hdrs = ["delta_record_stream_reader.h"],
    deps = [
        ":delta_record_reader",
        ":riegeli_stream_io",
        "//public/data_loading:records_utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "delta_record_stream_reader_test",
    srcs = ["delta_record_stream_reader_test.cc"],
    deps = [
        ":delta_record_stream_reader",
        "//public/data_loading:records_utils",
        "//public/data_loading/writers:delta_record_stream_writer",
        "@com_google_googletest//:gtest_main",
    ],
)
