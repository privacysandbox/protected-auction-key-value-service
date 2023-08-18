# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
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
    name = "request_simulation_executables",
    srcs = [
        "bin/start_request_simulation_system",
        "//tools/request_simulation:request_simulation_system_main",
    ],
    attributes = pkg_attributes(mode = "0555"),
    prefix = "/request_simulation/bin",
)

request_simulation_binaries = [
    ":request_simulation_executables",
]

pkg_zip(
    name = "request_simulation_binaries",
    srcs = request_simulation_binaries,
)

pkg_tar(
    name = "request_simulation_tar",
    srcs = request_simulation_binaries,
)

container_layer(
    name = "request_simulation_binary_layer",
    directory = "/",
    tars = [
        ":request_simulation_tar",
    ],
)

container_image(
    name = "request_simulation_docker_image",
    architecture = select({
        "@platforms//cpu:arm64": "arm64",
        "@platforms//cpu:x86_64": "amd64",
    }),
    base = select({
        "@platforms//cpu:arm64": "@runtime-debian-arm64//image",
        "@platforms//cpu:x86_64": "@runtime-debian-amd64//image",
    }),
    cmd = [
        "/request_simulation/bin/start_request_simulation_system",
    ],
    entrypoint = ["/bin/bash"],
    layers = [
        ":request_simulation_binary_layer",
    ],
)

pkg_zip(
    name = "request_simulation_artifacts",
    srcs = request_simulation_binaries,
)

genrule(
    name = "copy_to_dist",
    srcs = [
        ":request_simulation_artifacts",
        ":request_simulation_docker_image.tar",
    ],
    outs = ["copy_to_dist.bin"],
    cmd_bash = """cat << EOF > '$@'
mkdir -p dist/request_simulation/aws
cp $(execpath :request_simulation_artifacts) $(execpath :request_simulation_docker_image.tar) dist/request_simulation/aws
builders/tools/normalize-dist
EOF""",
    executable = True,
    local = True,
    message = "Copying request simulation artifacts to dist/request_simulation/aws directory",
)
