load("@bazel_skylib//rules:copy_file.bzl", "copy_file")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

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
        "//components/cloud_config:instance_client",
        "//components/cloud_config:parameter_client",
        "//components/data:blob_storage_client",
        "//components/data:delta_file_notifier",
        "//components/data_server/cache",
        "//components/data_server/data_loading:data_orchestrator",
        "//components/data_server/request_handler:get_values_handler",
        "//components/errors:retry",
        "//components/util:periodic_closure",
        "//components/util:version_linkstamp",
        "//public:base_types_cc_proto",
        "//public/data_loading/readers:riegeli_stream_io",
        "//public/query:get_values_cc_grpc",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_reflection",  # for grpc_cli
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/strings",
    ],
)

# copy "server"'s output as an opaque executable, simulating a binary that's
# not built from source
copy_file(
    name = "server_copy_exe",
    src = ":server",
    out = "server_copy.exe",
    is_executable = True,
)

sh_test(
    name = "server_buildinfo_test",
    size = "small",
    srcs = [":server_copy_exe"],
    args = ["--buildinfo"],
    tags = [
        "local",  # local precludes remote caching
        "smoke",
    ],
)
