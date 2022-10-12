load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "proto_matcher",
    testonly = 1,
    hdrs = ["proto_matcher.h"],
    deps = [
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = ["mocks.h"],
    deps = [
        "//public/data_loading/readers:riegeli_stream_io",
        "@com_google_googletest//:gtest",
    ],
)
