# JSON for Modern C++ https://github.com/nlohmann/json

load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["unencumbered"])  # Public Domain or MIT

exports_files(["LICENSE"])

cc_library(
    name = "lib",
    hdrs = [
        "single_include/nlohmann/json.hpp",
    ],
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    includes = ["single_include"],
    visibility = ["//visibility:public"],
)
