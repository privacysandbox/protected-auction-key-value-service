load("//third_party/flatbuffers:flatbuffers.bzl", "cc_flatbuffers_library", "flatbuffers_library")

package(
    default_applicable_licenses = ["//third_party/fledge_kv_server:license"],
    default_visibility = ["//visibility:public"],
)

flatbuffers_library(
    name = "data_loading_fbs",
    srcs = ["data_loading.fbs"],
)

cc_flatbuffers_library(
    name = "data_loading_cc_fbs",
    deps = [":data_loading_fbs"],
)
