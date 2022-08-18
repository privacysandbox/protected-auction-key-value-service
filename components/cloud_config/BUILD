package(default_visibility = [
    "//components:__subpackages__",
])

cc_library(
    name = "parameter_client",
    srcs = select({
        "//:aws_platform": ["aws_parameter_client.cc"],
    }),
    hdrs = [
        "parameter_client.h",
    ],
    deps = select({
        "//:aws_platform": [
            "@aws_sdk_cpp//:core",
            "@aws_sdk_cpp//:ssm",
            "//components/errors:aws_error_util",
        ],
        "//conditions:default": [],
    }) + [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/status",
        "@com_github_google_glog//:glog",
    ],
)

cc_library(
    name = "environment_client",
    srcs = select({
        "//:aws_parameters": ["aws_environment_client.cc"],
        "//:local_parameters": ["local_environment_client.cc"],
    }),
    hdrs = [
        "environment_client.h",
    ],
    deps = select({
        "//:aws_parameters": [
            "@aws_sdk_cpp//:core",
            "@aws_sdk_cpp//:ec2",
            "//components/errors:aws_error_util",
        ],
        "//:local_parameters": [
            "@com_google_absl//absl/flags:flag",
        ],
        "//conditions:default": [],
    }) + [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/status",
        "@com_github_google_glog//:glog",
    ],
)
