load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_image",
    "container_layer",
)
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

pkg_tar(
    name = "data_loading_analyzer_binaries",
    srcs = [
        ":data_loading_analyzer",
        "//components/aws/proxy/source:proxify",
        "//components/aws/proxy/source:proxy_preload",
    ],
    package_dir = "/opt/privacysandbox/bin",
)

container_layer(
    name = "data_loading_analyzer_layer",
    directory = "/",
    tars = [
        ":data_loading_analyzer_binaries",
    ],
)

# This image target is meant for testing running the server in an eclave using
# socat to abstract vsock communication.
#
# See project README.md on how to run the image.
container_image(
    name = "data_loading_analyzer_enclave_image",
    base = "@debian_slim_base_image//image",
    cmd = [
        "/opt/privacysandbox/bin/proxify",
        "/opt/privacysandbox/bin/data_loading_analyzer",
    ],
    entrypoint = ["/bin/bash"],
    layers = [
        ":data_loading_analyzer_layer",
    ],
)

cc_binary(
    name = "data_loading_analyzer",
    srcs = ["data_loading_analyzer.cc"],
    visibility = ["//production/packaging:__subpackages__"],
    deps = [
        "//components/data:blob_storage_client",
        "//components/data:delta_file_notifier",
        "//components/data:riegeli_stream_io",
        "//components/data_server/cache",
        "//components/data_server/data_loading:data_orchestrator",
        "//public:base_types_cc_proto",
        "//public/data_loading:data_loading_fbs",
        "@com_github_google_flatbuffers//:flatbuffers",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/strings",
    ],
)

cc_binary(
    name = "blob_storage_change_watcher",
    srcs = [
        "blob_storage_change_watcher.cc",
    ],
    deps = [
        "//components/data:blob_storage_change_notifier",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
    ],
)

cc_binary(
    name = "blob_storage_util",
    srcs = [
        "blob_storage_util.cc",
    ],
    deps = [
        "//components/data:blob_storage_client",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
    ],
)

cc_binary(
    name = "delta_file_watcher",
    srcs = [
        "delta_file_watcher.cc",
    ],
    deps = [
        "//components/data:blob_storage_client",
        "//components/data:delta_file_notifier",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
    ],
)
