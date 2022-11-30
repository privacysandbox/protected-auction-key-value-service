load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//components:__subpackages__"])

cc_library(
    name = "cache",
    srcs = [
        "key_value_cache.cc",
    ],
    hdrs = [
        "cache.h",
    ],
    deps = [
        "//public:base_types_cc_proto",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "key_value_cache_test",
    size = "small",
    srcs = [
        "key_value_cache_test.cc",
    ],
    deps = [
        ":cache",
        ":mocks",
        "//public:base_types_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = ["mocks.h"],
    deps = [
        ":cache",
        "@com_google_googletest//:gtest",
    ],
)
