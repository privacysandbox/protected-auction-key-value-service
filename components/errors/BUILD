load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = [
    "//components:__subpackages__",
])

cc_library(
    name = "aws_error_util",
    hdrs = [
        "aws_error_util.h",
    ],
    deps = [
        "@aws_sdk_cpp//:core",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "retry",
    srcs = [
        "retry.cc",
    ],
    hdrs = [
        "retry.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "retry_test",
    srcs = [
        "retry_test.cc",
    ],
    deps = [
        ":retry",
        "@com_google_googletest//:gtest_main",
    ],
)
