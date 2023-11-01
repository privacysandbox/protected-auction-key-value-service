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
    "container_layer",
)
load(
    "@rules_pkg//pkg:mappings.bzl",
    "pkg_attributes",
    "pkg_files",
)
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

pkg_files(
    name = "etc_resolv_files",
    srcs = [
        "//components/aws/resolv:resolv_config_group",
    ],
    attributes = pkg_attributes(mode = "0444"),
    prefix = "/etc",
)

pkg_tar(
    name = "resolv_config_tar",
    srcs = [
        ":etc_resolv_files",
    ],
)

container_layer(
    name = "resolv_config_layer",
    directory = "/",
    tars = [
        ":resolv_config_tar",
    ],
    visibility = ["//production/packaging:__subpackages__"],
)
