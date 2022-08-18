load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_layer",
)
load(
    "@rules_pkg//pkg:mappings.bzl",
    "pkg_attributes",
    "pkg_files",
)
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

pkg_files(
    name = "etc_resolv_files",
    srcs = [
        "//components/aws/resolv:resolv_config_group",
    ],
    attributes = pkg_attributes(mode = "0444"),
    prefix = "/etc",
)

pkg_tar(
    name = "resolv_config_tar",
    srcs = [
        ":etc_resolv_files",
    ],
)

container_layer(
    name = "resolv_config_layer",
    directory = "/",
    tars = [
        ":resolv_config_tar",
    ],
    visibility = ["//production/packaging:__subpackages__"],
)
