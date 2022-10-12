load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = [
    "//components:__subpackages__",
    "//tools:__subpackages__",
])

cc_library(
    name = "blob_storage_client",
    srcs = select({
        "//:aws_platform": ["s3_blob_storage_client.cc"],
    }),
    hdrs = [
        "blob_storage_client.h",
    ],
    deps = select({
        "//:aws_platform": [
            "@aws_sdk_cpp//:s3",
            "@aws_sdk_cpp//:transfer",
            "//components/errors:aws_error_util",
        ],
        "//conditions:default": [],
    }) + [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_github_google_glog//:glog",
    ],
)

cc_library(
    name = "blob_storage_change_notifier",
    srcs = select({
        "//:aws_platform": [
            "aws_sns_sqs_manager.cc",
            "aws_sns_sqs_manager.h",
            "s3_blob_storage_change_notifier.cc",
        ],
    }),
    hdrs = [
        "blob_storage_change_notifier.h",
    ],
    deps = select({
        "//:aws_platform": [
            "@aws_sdk_cpp//:sns",
            "@aws_sdk_cpp//:sqs",
            "//components/errors:aws_error_util",
        ],
    }) + [
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_github_google_glog//:glog",
    ],
)

cc_library(
    name = "delta_file_notifier",
    srcs = [
        "delta_file_notifier.cc",
    ],
    hdrs = [
        "delta_file_notifier.h",
    ],
    deps = [
        ":blob_storage_change_notifier",
        ":blob_storage_client",
        "//public:constants",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "delta_file_notifier_test",
    srcs = [
        "delta_file_notifier_test.cc",
    ],
    deps = [
        ":delta_file_notifier",
        ":mocks",
        "//public/data_loading:filename_utils",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = ["mocks.h"],
    deps = [
        ":blob_storage_client",
        ":delta_file_notifier",
        "@com_google_googletest//:gtest",
    ],
)
