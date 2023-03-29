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
    name = "data_cli",
    srcs = ["//tools/data_cli"],
    attributes = pkg_attributes(mode = "0555"),
    prefix = "/tools/data_cli",
)

pkg_files(
    name = "benchmarks",
    srcs = [
        "//components/tools/benchmarks:data_loading_benchmark",
    ],
    attributes = pkg_attributes(mode = "0555"),
    prefix = "/tools/benchmarks",
)

pkg_files(
    name = "realtime",
    srcs = [
        "//components/tools:realtime_updates_publisher",
    ],
    attributes = pkg_attributes(mode = "0555"),
    prefix = "/tools/realtime",
)

tools_binaries = [
    ":benchmarks",
    ":data_cli",
    ":realtime",
]

pkg_zip(
    name = "tools_binaries_zip",
    srcs = tools_binaries,
)

pkg_tar(
    name = "tools_binaries_tar",
    srcs = tools_binaries,
)

container_layer(
    name = "tools_binaries_layer",
    directory = "/",
    tars = [
        ":tools_binaries_tar",
    ],
)

container_image(
    name = "tools_binaries_docker_image",
    architecture = select({
        "@platforms//cpu:arm64": "arm64",
        "@platforms//cpu:x86_64": "amd64",
    }),
    base = select({
        "@platforms//cpu:arm64": "@runtime-debian-arm64//image",
        "@platforms//cpu:x86_64": "@runtime-debian-amd64//image",
    }),
    layers = [
        ":tools_binaries_layer",
    ],
)

genrule(
    name = "copy_to_dist",
    srcs = [
        ":tools_binaries_zip",
        ":tools_binaries_docker_image.tar",
    ],
    outs = ["copy_to_dist.bin"],
    cmd_bash = """cat << EOF > '$@'
mkdir -p dist/debian
cp $(execpath :tools_binaries_zip) dist
cp $(execpath :tools_binaries_docker_image.tar) dist
builders/tools/normalize-dist
EOF""",
    executable = True,
    local = True,
    message = "Copying tools to dist/debian directory",
)
