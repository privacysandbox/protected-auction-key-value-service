load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = [
    "//components:__subpackages__",
])

cc_library(
    name = "duration",
    srcs = ["duration.cc"],
    hdrs = ["duration.h"],
    deps = [
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "duration_test",
    size = "small",
    srcs = ["duration_test.cc"],
    deps = [
        ":duration",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "periodic_closure",
    srcs = [
        "periodic_closure.cc",
    ],
    hdrs = ["periodic_closure.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "periodic_closure_test",
    size = "small",
    srcs = ["periodic_closure_test.cc"],
    deps = [
        ":periodic_closure",
        "@com_google_googletest//:gtest_main",
    ],
)

local_defines = select({
    "//:aws_instance": ["INSTANCE=\\\"aws\\\""],
    "//:local_instance": ["INSTANCE=\\\"local\\\""],
    "//conditions:default": ["INSTANCE=\\\"unknown\\\""],
}) + select({
    "//:aws_platform": ["PLATFORM=\\\"aws\\\""],
    "//:local_platform": ["PLATFORM=\\\"local\\\""],
    "//conditions:default": ["PLATFORM=\\\"unknown\\\""],
})

genrule(
    name = "generate_version_linkstamp",
    outs = ["version_linkstamp.cc"],
    cmd = """
        $(location :generate_version_h) "$(TARGET_CPU)-$(COMPILATION_MODE)" >>$@
    """,
    # Undocumented attr to depend on workspace status files.
    # Used here because generate_version_h depends on the workspace status files.
    stamp = 1,
    tools = [":generate_version_h"],
    visibility = ["//visibility:private"],
)

cc_library(
    name = "build_flags",
    srcs = [
        "build_flags.cc",
        "build_info.cc",
    ],
    hdrs = [
        "build_info.h",
    ],
    linkstamp = ":generate_version_linkstamp",
    local_defines = local_defines,
    visibility = ["//visibility:private"],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "version_linkstamp",
    # Linking this library makes build cache inefficient, limiting this to the //components/data_server/server package only.
    visibility = ["//components/data_server/server:__pkg__"],
    deps = [":build_flags"],
    alwayslink = 1,
)
