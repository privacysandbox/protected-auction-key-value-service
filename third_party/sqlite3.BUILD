load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "sqlite3",
    srcs = ["sqlite3.c"],
    hdrs = ["sqlite3.h"],
    linkopts = ["-ldl"],
)
