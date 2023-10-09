# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
        "//components/data_server/server",
        "//components/tools:get_region",
        "@google_privacysandbox_servers_common//scp/cc/aws/proxy/src:proxify",
        "@google_privacysandbox_servers_common//scp/cc/aws/proxy/src:socket_vendor",
    ],
    attributes = pkg_attributes(mode = "0555"),
    prefix = "/",
)

pkg_files(
    name = "server_libs",
    srcs = [
        "@google_privacysandbox_servers_common//scp/cc/aws/proxy/src:proxy_preload",
    ],
    attributes = pkg_attributes(mode = "0444"),
    prefix = "/",
)

pkg_files(
    name = "kmstool_enclave_executable",
    srcs = [
        "@kmstool_enclave_cli//file",
    ],
    attributes = pkg_attributes(mode = "0555"),
    prefix = "/",
)

server_binaries = [
    ":kmstool_enclave_executable",
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

# Ensure libnsm ends up in the load path.
pkg_tar(
    name = "libnsm-tar",
    srcs = [
        "@libnsm//file",
    ],
    mode = "0644",
    package_dir = "/lib/x86_64-linux-gnu/",
    visibility = ["//visibility:public"],
)

container_layer(
    name = "server_binary_layer",
    directory = "/",
    tars = [
        ":libnsm-tar",
        ":server_binaries_tar",
    ],
)

# Create a symlink between where kmstool_enclave_cli expects shell to be
# (/bin/sh) and where it actually is on our image (/busybox/sh).
container_layer(
    name = "kmstool_enclave_cli_layer",
    symlinks = {
        "/bin/sh": "/busybox/sh",
    },
    tars = [
        ":libnsm-tar",
    ],
)

# This image target is meant for testing running the server in an enclave using.
#
# See project README.md on how to run the image.
container_image(
    name = "server_docker_image",
    architecture = select({
        "@platforms//cpu:arm64": "arm64",
        "@platforms//cpu:x86_64": "amd64",
    }),
    base = select({
        "@platforms//cpu:arm64": "@runtime-debian-debug-nonroot-arm64//image",
        "@platforms//cpu:x86_64": "@runtime-debian-debug-nonroot-amd64//image",
    }),
    cmd = [
        "/init_server_enclave",
    ],
    entrypoint = ["/bin/bash"],
    layers = [
        "//production/packaging/aws/resolv:resolv_config_layer",
        ":server_binary_layer",
        ":kmstool_enclave_cli_layer",
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
        "//public/query:query_api_descriptor_set",
    ],
    outs = ["copy_to_dist.bin"],
    cmd_bash = """cat << EOF > '$@'
mkdir -p dist/debian
cp $(execpath :server_artifacts) dist/debian
cp $(execpath :server_docker_image.tar) $(execpath //public/query:query_api_descriptor_set) dist
# retain previous server_docker_image.tar location as a symlink
ln -rsf dist/server_docker_image.tar dist/debian/server_docker_image.tar
builders/tools/normalize-dist
EOF""",
    executable = True,
    local = True,
    message = "copying server artifacts to dist directory",
)
