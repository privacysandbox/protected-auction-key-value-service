load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "cbor",
    srcs = glob([
        "allocators.c",
        "cbor.c",
        "cbor/**/*.c",
        "cbor/**/*.h",
    ]),
    hdrs = [
        "cbor.h",
    ],
    visibility = ["//visibility:public"],
)
