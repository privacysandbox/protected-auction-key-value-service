"""Build rules for release-related tools."""

load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_image",
    "container_layer",
)
load("@io_bazel_rules_docker//contrib:test.bzl", "container_test")
load(
    "@io_bazel_rules_docker//docker/util:run.bzl",
    "container_run_and_commit",
    "container_run_and_commit_layer",
)
load(
    "@rules_pkg//pkg:mappings.bzl",
    "pkg_attributes",
    "pkg_files",
)
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

pkg_files(
    name = "git_config",
    srcs = [
        ".gitconfig",
    ],
    attributes = pkg_attributes(mode = "0400"),
    prefix = "/root",
)

pkg_tar(
    name = "release_files",
    srcs = [
        ":git_config",
    ],
)

container_layer(
    name = "release_config_layer",
    directory = "/",
    tars = [
        ":release_files",
    ],
)

# container image in which to run release tools
container_run_and_commit_layer(
    name = "release_layer",
    commands = [
        """
        npm install --global release-please commit-and-tag-version

        # Install the GitHub CLI tool (https://cli.github.com/)
        apt-get update
        apt-get install apt-transport-https
        mkdir -p /etc/apt/keyrings
        curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | dd of=/etc/apt/keyrings/githubcli-archive-keyring.gpg
        echo 'deb [arch=amd64 signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main' | tee /etc/apt/sources.list.d/github-cli.list
        chmod 644 /etc/apt/keyrings/githubcli-archive-keyring.gpg /etc/apt/sources.list.d/github-cli.list
        apt-get update
        apt-get install gh
        """,
    ],
    docker_run_flags = [
        # reset the npm image's entrypoint, as `container_run_and_commit()` invokes `sh`
        "--entrypoint=",
    ],
    image = "@gcr_cloud_builders_npm//image",
    tags = [
        "manual",  # Exclude from wildcard queries.
        "no-remote",
    ],
)

container_image(
    name = "release_image",
    base = "@gcr_cloud_builders_npm//image",
    entrypoint = "/bin/bash",
    layers = [
        ":release_layer",
        ":release_config_layer",
    ],
    workdir = "/",
)

container_test(
    name = "structure_test",
    size = "medium",
    configs = ["test/structure.yaml"],
    driver = "tar",
    image = ":release_image",
)

container_test(
    name = "commands_test",
    size = "small",
    configs = ["test/commands.yaml"],
    driver = "docker",
    image = ":release_image",
)
