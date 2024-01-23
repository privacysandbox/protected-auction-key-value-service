# Copied from https://github.com/tensorflow/io/blob/2d20cf378229ffa28b4bc999cb65e3b0c11a0ea6/third_party/avro.BUILD
# Description:
#   Avro Library

load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE.txt"])

copts = [
    "-fexceptions",
    "-Wno-error",
    "-Wno-implicit-fallthrough",
    "-Wno-non-virtual-dtor",
]

cc_library(
    name = "avrocpp",
    srcs = glob(
        [
            "third_party/avro/impl/**/*.hh",
            "third_party/avro/impl/**/*.cc",
        ],
        exclude = [
            "third_party/avro/impl/avrogencpp.cc",
        ],
    ),
    hdrs = glob(
        [
            "third_party/avro/api/**/*.hh",
        ],
    ),
    copts = copts,
    includes = [
        "third_party/avro/api",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@boost//:any",
        "@boost//:asio",
        "@boost//:assign",
        "@boost//:crc",
        "@boost//:format",
    ],
)
