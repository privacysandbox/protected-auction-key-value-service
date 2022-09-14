load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "zlib",
    srcs = glob([
        "*.c",
        "*.h",
    ]),
    hdrs = ["zlib.h"],
    copts = [
        "-Wno-shift-negative-value",
        "-DZ_HAVE_UNISTD_H",
    ],
    includes = ["."],
)
