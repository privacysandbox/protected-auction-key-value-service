load("@io_bazel_rules_docker//docker/util:run.bzl", "container_run_and_commit_layer")

container_run_and_commit_layer(
    name = "socat_and_ip_commands_layer",
    commands = [
        # Install socat and IP command
        "apt-get update",
        "apt-get install -y socat iproute2",
    ],
    image = select({
        "@platforms//cpu:arm64": "@runtime-debian-arm64//image",
        "@platforms//cpu:x86_64": "@runtime-debian-amd64//image",
    }),
    tags = ["no-remote"],
    visibility = ["//production/packaging:__subpackages__"],
)
