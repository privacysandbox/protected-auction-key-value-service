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
load("@rules_pkg//pkg:zip.bzl", "pkg_zip")

pkg_files(
    name = "server_executables",
    srcs = [
        "bin/init_server_basic",
        "bin/init_server_enclave",
        "//components/aws/proxy/source:proxify",
        "//components/data_server/server",
        "//components/tools:get_region",
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

server_binaries = [
    ":server_executables",
    ":server_libs",
]

pkg_zip(
    name = "server_binaries",
    srcs = server_binaries,
)

pkg_tar(
    name = "server_binaries_tar",
    srcs = server_binaries,
)

container_layer(
    name = "server_binary_layer",
    directory = "/",
    tars = [
        ":server_binaries_tar",
    ],
)

# This image target is meant for testing running the server in an enclave using
# socat to abstract vsock communication.
#
# See project README.md on how to run the image.
container_image(
    name = "server_docker_image",
    architecture = select({
        "@platforms//cpu:arm64": "arm64",
        "@platforms//cpu:x86_64": "amd64",
    }),
    base = select({
        "@platforms//cpu:arm64": "@runtime-debian-arm64//image",
        "@platforms//cpu:x86_64": "@runtime-debian-amd64//image",
    }),
    cmd = [
        "/server/bin/init_server_enclave",
    ],
    entrypoint = ["/bin/bash"],
    layers = [
        "//production/packaging/envoy_proxy:envoy_distroless_layer",
        "//production/packaging/envoy_proxy:envoy_config_layer",
        "//production/packaging/aws/socat:socat_and_ip_commands_layer",
        "//production/packaging/aws/resolv:resolv_config_layer",
        ":server_binary_layer",
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

# server artifacts
pkg_zip(
    name = "server_artifacts",
    srcs = server_binaries,
)

genrule(
    name = "copy_to_dist",
    srcs = [
        ":server_artifacts",
        ":server_docker_image.tar",
        "//public/query:get_values_descriptor_set",
    ],
    outs = ["copy_to_dist.bin"],
    cmd_bash = """cat << EOF > '$@'
mkdir -p dist/debian
cp $(execpath :server_artifacts) $(execpath :server_docker_image.tar) dist/debian
cp $(execpath //public/query:get_values_descriptor_set) dist
builders/tools/normalize-dist
EOF""",
    executable = True,
    local = True,
    message = "copying server artifacts to dist/debian directory",
)
