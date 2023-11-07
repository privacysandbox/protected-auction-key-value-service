load(
    "//third_party/fledge_kv_server/google_internal/piper:configure_copts.bzl",
    "COPTS",
)

package(
    default_applicable_licenses = ["//third_party/fledge_kv_server:license"],
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "proto_matcher",
    testonly = 1,
    hdrs = ["proto_matcher.h"],
    copts = COPTS,
    deps = [
        "//third_party/fledge_kv_server/google_internal/piper:gtest",
        "//third_party/fledge_kv_server/google_internal/piper:message_differencer",
    ],
)
