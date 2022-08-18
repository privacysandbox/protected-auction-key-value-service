cc_library(
    name = "key_value_service_impl",
    srcs = [
        "key_value_service_impl.cc",
    ],
    hdrs = [
        "key_value_service_impl.h",
    ],
    deps = [
        "//components/data_server/cache",
        "//components/data_server/request_handler:get_values_handler",
        "//public/query:get_values_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_binary(
    name = "server",
    srcs = ["server.cc"],
    visibility = ["//production/packaging:__subpackages__"],
    deps = [
        ":key_value_service_impl",
        "//components/cloud_config:environment_client",
        "//components/cloud_config:parameter_client",
        "//components/data:blob_storage_client",
        "//components/data:delta_file_notifier",
        "//components/data:riegeli_stream_io",
        "//components/data_server/cache",
        "//components/data_server/data_loading:data_orchestrator",
        "//components/data_server/request_handler:get_values_handler",
        "//components/errors:retry",
        "//public:base_types_cc_proto",
        "//public/query:get_values_cc_grpc",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_reflection",  # for grpc_cli
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/strings",
    ],
)
