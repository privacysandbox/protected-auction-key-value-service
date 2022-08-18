load("@io_bazel_rules_docker//docker/util:run.bzl", "container_run_and_commit_layer")

container_run_and_commit_layer(
    name = "socat_and_ip_commands_layer",
    commands = [
        # Install socat and IP command
        "apt-get update",
        "apt-get install -y socat iproute2",
    ],
    image = "@debian_slim_base_image//image",
    tags = ["no-remote"],
    visibility = ["//production/packaging:__subpackages__"],
)
