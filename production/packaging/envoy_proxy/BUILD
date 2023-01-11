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
    "container_flatten",
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
    ],
    attributes = pkg_attributes(mode = "0444"),
    prefix = "/etc/envoy",
)

pkg_files(
    name = "etc_envoy_config_files",
    srcs = [
        "//public/query:query_api_descriptor_set",
    ],
    attributes = pkg_attributes(mode = "0444"),
    prefix = "/etc/envoy/config",
)

pkg_tar(
    name = "envoy_config_tar",
    srcs = [
        ":etc_envoy_config_files",
        ":etc_envoy_files",
    ],
)

container_layer(
    name = "envoy_config_layer",
    directory = "/",
    tars = [
        ":envoy_config_tar",
    ],
    visibility = ["//production/packaging:__subpackages__"],
)

container_flatten(
    name = "envoy_distroless_flat",
    image = select({
        "@platforms//cpu:arm64": "@envoy-distroless-arm64//image",
        "@platforms//cpu:x86_64": "@envoy-distroless-amd64//image",
    }),
)

container_layer(
    name = "envoy_distroless_layer",
    tars = [
        ":envoy_distroless_flat.tar",
    ],
    visibility = ["//production/packaging:__subpackages__"],
)
