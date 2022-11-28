load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//public:__subpackages__"])

cc_library(
    name = "record_aggregator",
    srcs = ["record_aggregator.cc"],
    hdrs = ["record_aggregator.h"],
    deps = [
        "//public/data_loading:data_loading_fbs",
        "//public/data_loading:records_utils",
        "//public/data_loading/writers:delta_record_stream_writer",
        "//public/data_loading/writers:delta_record_writer",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@sqlite3",
    ],
)

cc_test(
    name = "record_aggregator_test",
    srcs = ["record_aggregator_test.cc"],
    deps = [
        ":record_aggregator",
        "//public/data_loading:records_utils",
        "//public/data_loading/readers:delta_record_stream_reader",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
