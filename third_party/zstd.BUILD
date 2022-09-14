load("@rules_cc//cc:defs.bzl", "cc_library")

package(
    default_visibility = ["//visibility:public"],
    features = ["header_modules"],
)

licenses(["notice"])

cc_library(
    name = "zstdlib",
    srcs = glob([
        "common/*.c",
        "common/*.h",
        "compress/*.c",
        "compress/*.h",
        "decompress/*.c",
        "decompress/*.h",
        "decompress/*.S",
    ]),
    hdrs = [
        "zdict.h",
        "zstd.h",
        "zstd_errors.h",
    ],
)
