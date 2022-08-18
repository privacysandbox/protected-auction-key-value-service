load("@io_bazel_rules_docker//container:container.bzl", "container_image")
load("@io_bazel_rules_docker//contrib:test.bzl", "container_test")
load("@io_bazel_rules_docker//docker/util:run.bzl", "container_run_and_commit_layer")

# Target for the Bazel Build Image
#
# This image is useful for building container targets that run in a predictable
# manner (i.e., avoids dependency issues that may arise from building a
# container target on one platform/architecture combo and then trying to run it
# on another platform/architecture combo).
# See README.md for instructions on how to build and use the output image.

bazel_version = "5.2.0"

# validate_bazel_ver checks (when invoked via build or test) that the bazel_version variable declared above is the same value as
# contained in `.bazelversion` in the top-level workspace directory
genrule(
    name = "validate_bazel_ver",
    srcs = [],
    outs = ["show_predefined_variables.sh"],
    cmd_bash = """cat << EOF > $@
#!/usr/bin/env bash
if [[ {ver} != {dot_bazelversion} ]]; then
  cat << ERR > /dev/stderr
Bazel version mismatch:
    bazel_version: {ver}
    /.bazelversion: {dot_bazelversion}
ERR
  exit 1
fi
EOF
bash $@
""".format(
        dot_bazelversion = "$$(cat $(execpath //:.bazelversion))",
        ver = bazel_version,
    ),
    tools = [
        "//:.bazelversion",
    ],
)

container_run_and_commit_layer(
    name = "bazel_build_image_layer",
    commands = [
        """
        apt-get update
        apt-get install -y lsb-release clang ca-certificates curl gnupg software-properties-common""",
        """
        # Install bazelisk and execute specified version of bazel
        go install github.com/bazelbuild/bazelisk@latest
        ln -s /go/bin/bazelisk /usr/bin/bazel
        # install the specified version of bazel
        /usr/bin/bazel version""",
        """
        # Install Docker (https://docs.docker.com/engine/install/debian/)
        apt-get remove docker docker.io containerd runc
        mkdir -p /etc/apt/keyrings
        curl -fsSL https://download.docker.com/linux/debian/gpg | gpg --dearmor -o /etc/apt/keyrings/docker.gpg
        echo 'deb [arch=amd64 signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/debian bullseye stable' | tee /etc/apt/sources.list.d/docker.list
        apt-get update
        apt-get install -y docker-ce docker-ce-cli containerd.io""",
        """
        # Install Packer (https://learn.hashicorp.com/tutorials/packer/get-started-install-cli)
        curl -fsSL https://apt.releases.hashicorp.com/gpg | apt-key add -
        apt-add-repository 'deb [arch=amd64] https://apt.releases.hashicorp.com bullseye main'
        apt-get update && apt-get install packer""",
    ],
    docker_run_flags = [
        "--workdir=/root",
        "--env=USE_BAZEL_VERSION=" + bazel_version,
    ],
    image = "@golang_debian_bullseye_image//image",
    tags = [
        "manual",  # Exclude from wildcard queries.
        "no-remote",
    ],
)

container_image(
    name = "bazel_build_image",
    base = "@golang_debian_bullseye_image//image",
    entrypoint = "/bin/bash",
    layers = [
        ":bazel_build_image_layer",
    ],
    workdir = "/",
)

container_test(
    name = "commands_test",
    size = "small",
    configs = ["test/commands.yaml"],
    driver = "docker",
    image = ":bazel_build_image",
)
