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

pkg_files(
    name = "etc_envoy_files",
    srcs = [
        "//components/envoy_proxy:envoy_proxy_config_group",
        "//public/query:query_api_descriptor_set",
    ],
    attributes = pkg_attributes(mode = "0444"),
    prefix = "/etc/envoy",
)

pkg_tar(
    name = "envoy_config_tar",
    srcs = [
        ":etc_envoy_files",
    ],
)

container_layer(
    name = "envoy_config_layer",
    directory = "/",
    tars = [
        ":envoy_config_tar",
    ],
)

container_image(
    name = "envoy_image",
    architecture = select({
        "@platforms//cpu:arm64": "arm64",
        "@platforms//cpu:x86_64": "amd64",
    }),
    base = select({
        "@platforms//cpu:arm64": "@envoy-distroless-arm64//image",
        "@platforms//cpu:x86_64": "@envoy-distroless-amd64//image",
    }),
    layers = [
        ":envoy_config_layer",
    ],
)

genrule(
    name = "build_envoy_image",
    srcs = [
        ":envoy_image.tar",
    ],
    outs = ["build_envoy_image.bin"],
    cmd_bash = """cat << EOF > '$@'
mkdir -p testing/run_local/dist
cp $(execpath :envoy_image.tar) testing/run_local/dist
EOF""",
    executable = True,
    local = True,
    message = "copying envoy image to testing/run_local/dist directory",
)
