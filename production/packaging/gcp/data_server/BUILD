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
        "//components/data_server/server",
    ],
    attributes = pkg_attributes(mode = "0555"),
    prefix = "/",
)

server_binaries = [
    ":server_executables",
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
        "@platforms//cpu:arm64": "@runtime-debian-debug-root-arm64//image",
        "@platforms//cpu:x86_64": "@runtime-debian-debug-root-amd64//image",
    }),
    entrypoint = ["/init_server_basic"],
    labels = {"tee.launch_policy.log_redirect": "always"},
    layers = [
        ":server_binary_layer",
    ],
    ports = ["50051"],
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
    message = "copying server artifacts to dist/debian directory",
)
