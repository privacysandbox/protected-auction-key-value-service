load("//builders/amazonlinux2:nitro.bzl", "nitro_enclave_image")
load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_image",
    "container_layer",
)
load("@io_bazel_rules_docker//contrib:test.bzl", "container_test")
load(
    "@rules_pkg//pkg:mappings.bzl",
    "pkg_attributes",
    "pkg_files",
)
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

pkg_files(
    name = "server_executables",
    srcs = glob(["bin/init_server_*"]) + [
        "//components/data_server/server",
    ],
    attributes = pkg_attributes(mode = "0555"),
    prefix = "/server/bin",
)

pkg_files(
    name = "proxy_executables",
    srcs = [
        "//components/aws/proxy/source:proxify",
    ],
    attributes = pkg_attributes(mode = "0555"),
    prefix = "/server/bin",
)

pkg_files(
    name = "server_libs",
    srcs = [
        "//components/aws/proxy/source:proxy_preload",
    ],
    attributes = pkg_attributes(mode = "0444"),
    prefix = "/server/bin",
)

pkg_tar(
    name = "server_binaries",
    srcs = [
        ":proxy_executables",
        ":server_executables",
        ":server_libs",
    ],
)

container_layer(
    name = "server_binary_layer",
    directory = "/",
    tars = [
        ":server_binaries",
    ],
)

# This image target is meant for testing running the server in an enclave using
# socat to abstract vsock communication.
#
# See project README.md on how to run the image.
container_image(
    name = "server_docker_image",
    base = "@debian_slim_base_image//image",
    cmd = [
        "/server/bin/init_server_enclave",
    ],
    entrypoint = ["/bin/bash"],
    layers = [
        ":server_binary_layer",
        "//production/packaging/envoy_proxy:envoy_distroless_layer",
        "//production/packaging/envoy_proxy:envoy_config_layer",
        "//production/packaging/aws/socat:socat_and_ip_commands_layer",
        "//production/packaging/aws/resolv:resolv_config_layer",
    ],
)

container_test(
    name = "structure_test",
    size = "medium",
    configs = ["test/structure.yaml"],
    driver = "tar",
    image = ":server_docker_image",
)

container_test(
    name = "commands_test",
    size = "small",
    configs = ["test/commands.yaml"],
    driver = "docker",
    image = ":server_docker_image",
)

# build an EIF for the data server
nitro_enclave_image(
    name = "server_enclave_image",
    aws_nitro_image = "//builders/amazonlinux2:nitro_build_image.tar",
    source_image = ":server_docker_image.tar",
    tags = [
        "manual",  # Exclude from wildcard queries.
        "no-remote",
    ],
    target_eif_basename = "server_enclave_image",
)
