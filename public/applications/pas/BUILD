load(
    "//third_party/fledge_kv_server/google_internal/piper:configure_copts.bzl",
    "COPTS",
)

package(
    default_applicable_licenses = ["//third_party/fledge_kv_server:license"],
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "retrieval_request_builder",
    srcs = ["retrieval_request_builder.cc"],
    hdrs = ["retrieval_request_builder.h"],
    copts = COPTS,
    deps = [
        "//third_party/absl/container:flat_hash_map",
        "//third_party/fledge_kv_server/google_internal/piper/public/query/v2:get_values_v2_cc_proto",
    ],
)

cc_library(
    name = "retrieval_response_parser",
    srcs = ["retrieval_response_parser.cc"],
    hdrs = ["retrieval_response_parser.h"],
    copts = COPTS,
    deps = [
        "//third_party/absl/status",
        "//third_party/absl/status:statusor",
        "//third_party/absl/strings",
        "//third_party/fledge_kv_server/google_internal/piper/public/query/v2:get_values_v2_cc_proto",
    ],
)
