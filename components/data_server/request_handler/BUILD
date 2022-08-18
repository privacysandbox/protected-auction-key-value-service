package(default_visibility = [
    "//components/data_server:__subpackages__",
])

cc_library(
    name = "get_values_handler",
    srcs = [
        "get_values_handler.cc",
    ],
    hdrs = [
        "get_values_handler.h",
    ],
    deps = [
        "//components/data_server/cache",
        "//public:base_types_cc_proto",
        "//public:constants",
        "//public/query:get_values_cc_grpc",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "get_values_handler_test",
    srcs = [
        "get_values_handler_test.cc",
    ],
    deps = [
        ":get_values_handler",
        "//components/data_server/cache",
        "//components/data_server/cache:mocks",
        "//components/test_util:proto_matcher",
        "//public/query:get_values_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googletest//:gtest_main",
    ],
)
