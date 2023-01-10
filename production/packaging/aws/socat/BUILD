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
