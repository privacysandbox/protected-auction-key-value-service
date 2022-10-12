load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = [
    "//components:__subpackages__",
])

cc_library(
    name = "bhttp_utils",
    hdrs = [
        "bhttp_utils.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_github_google_quiche//quiche:binary_http_unstable_api",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "bhttp_utils_test",
    srcs = [
        "bhttp_utils_test.cc",
    ],
    deps = [
        ":bhttp_utils",
        "@com_github_google_quiche//quiche:binary_http_unstable_api",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
