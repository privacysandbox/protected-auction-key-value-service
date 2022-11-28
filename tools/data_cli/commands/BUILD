load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = [
    "//tools:__subpackages__",
])

cc_library(
    name = "command",
    hdrs = ["command.h"],
    deps = [],
)

cc_library(
    name = "generate_data_command",
    srcs = ["generate_data_command.cc"],
    hdrs = ["generate_data_command.h"],
    deps = [
        ":command",
        "//public/data_loading/csv:csv_delta_record_stream_reader",
        "//public/data_loading/csv:csv_delta_record_stream_writer",
        "//public/data_loading/readers:delta_record_reader",
        "//public/data_loading/readers:delta_record_stream_reader",
        "//public/data_loading/writers:delta_record_stream_writer",
        "//public/data_loading/writers:delta_record_writer",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "generate_data_command_test",
    size = "small",
    srcs = ["generate_data_command_test.cc"],
    deps = [
        ":generate_data_command",
        "//public/data_loading/csv:csv_delta_record_stream_reader",
        "//public/data_loading/csv:csv_delta_record_stream_writer",
        "//public/data_loading/readers:delta_record_stream_reader",
        "//public/data_loading/writers:delta_record_stream_writer",
        "@com_google_googletest//:gtest_main",
    ],
)
