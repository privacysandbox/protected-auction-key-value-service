"""Build rules for AWS Nitro Enclave Images."""

load("@io_bazel_rules_docker//container:container.bzl", "container_image")
load("@io_bazel_rules_docker//contrib:test.bzl", "container_test")
load("@io_bazel_rules_docker//docker/util:run.bzl", "container_run_and_commit_layer")
load(
    "@rules_pkg//pkg:mappings.bzl",
    "pkg_attributes",
    "pkg_files",
)
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

pkg_files(
    name = "scripts",
    srcs = glob(["install_*"]),
    attributes = pkg_attributes(mode = "0755"),
    prefix = "/scripts",
)

pkg_files(
    name = "configs",
    srcs = ["//:.bazelversion"],
    attributes = pkg_attributes(mode = "0444"),
    prefix = "/",
)

pkg_tar(
    name = "extra_files",
    srcs = [
        ":configs",
        ":scripts",
    ],
)

container_image(
    name = "amazonlinux2_with_extras",
    base = "@amazonlinux2_image//image",
    tars = [":extra_files"],
    workdir = "/",
)

# container image in which to convert a docker image to an EIF image
container_run_and_commit_layer(
    name = "nitro_build_layer",
    commands = [
        """
        /scripts/install_yum_apps --with-golang --with-clang
        echo Y | amazon-linux-extras install aws-nitro-enclaves-cli
        yum install -y aws-nitro-enclaves-cli-devel
        """,
    ],
    image = ":amazonlinux2_with_extras.tar",
    tags = [
        "manual",  # Exclude from wildcard queries.
        "no-remote",
    ],
)

container_image(
    name = "nitro_build_image",
    base = "@amazonlinux2_image//image",
    entrypoint = "/bin/bash",
    layers = [
        ":nitro_build_layer",
    ],
    visibility = ["//visibility:public"],
    workdir = "/",
)

container_test(
    name = "commands_test",
    size = "small",
    configs = ["test/commands.yaml"],
    driver = "docker",
    image = ":nitro_build_image",
)

exports_files(["generate_nitro.sh.tpl"])
