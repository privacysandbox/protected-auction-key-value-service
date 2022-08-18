load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_flatten",
    "container_layer",
)
load(
    "@rules_pkg//pkg:mappings.bzl",
    "pkg_attributes",
    "pkg_files",
)
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

pkg_files(
    name = "etc_envoy_files",
    srcs = [
        "//components/envoy_proxy:envoy_proxy_config_group",
    ],
    attributes = pkg_attributes(mode = "0444"),
    prefix = "/etc/envoy",
)

pkg_files(
    name = "etc_envoy_config_files",
    srcs = [
        "//public/query:get_values_descriptor_set",
    ],
    attributes = pkg_attributes(mode = "0444"),
    prefix = "/etc/envoy/config",
)

pkg_tar(
    name = "envoy_config_tar",
    srcs = [
        ":etc_envoy_config_files",
        ":etc_envoy_files",
    ],
)

container_layer(
    name = "envoy_config_layer",
    directory = "/",
    tars = [
        ":envoy_config_tar",
    ],
    visibility = ["//production/packaging:__subpackages__"],
)

container_flatten(
    name = "envoy_distroless_flat",
    image = "@envoy-distroless//image",
)

container_layer(
    name = "envoy_distroless_layer",
    tars = [
        ":envoy_distroless_flat.tar",
    ],
    visibility = ["//production/packaging:__subpackages__"],
)
